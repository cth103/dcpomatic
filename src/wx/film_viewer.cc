/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/film_viewer.cc
 *  @brief A wx widget to view a preview of a Film.
 */

#include "film_viewer.h"
#include "playhead_to_timecode_dialog.h"
#include "playhead_to_frame_dialog.h"
#include "wx_util.h"
#include "closed_captions_dialog.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include "lib/job_manager.h"
#include "lib/image.h"
#include "lib/exceptions.h"
#include "lib/examine_content_job.h"
#include "lib/filter.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/video_content.h"
#include "lib/video_decoder.h"
#include "lib/timer.h"
#include "lib/butler.h"
#include "lib/log.h"
#include "lib/config.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <dcp/exceptions.h>
#include <wx/tglbtn.h>
#include <iostream>
#include <iomanip>

using std::string;
using std::pair;
using std::min;
using std::max;
using std::cout;
using std::list;
using std::bad_alloc;
using std::make_pair;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::weak_ptr;
using boost::optional;
using dcp::Size;

static
int
rtaudio_callback (void* out, void *, unsigned int frames, double, RtAudioStreamStatus, void* data)
{
	return reinterpret_cast<FilmViewer*>(data)->audio_callback (out, frames);
}

FilmViewer::FilmViewer (wxWindow* p)
	: _panel (new wxPanel (p))
	, _coalesce_player_changes (false)
	, _audio (DCPOMATIC_RTAUDIO_API)
	, _audio_channels (0)
	, _audio_block_size (1024)
	, _playing (false)
	, _latency_history_count (0)
	, _dropped (0)
	, _closed_captions_dialog (new ClosedCaptionsDialog(p))
	, _outline_content (false)
	, _eyes (EYES_LEFT)
{
#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif

	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);

	_panel->Bind            (wxEVT_PAINT,               boost::bind (&FilmViewer::paint_panel,     this));
	_panel->Bind            (wxEVT_SIZE,                boost::bind (&FilmViewer::panel_sized,     this, _1));
	_timer.Bind             (wxEVT_TIMER,               boost::bind (&FilmViewer::timer,           this));

	set_film (shared_ptr<Film> ());

	_config_changed_connection = Config::instance()->Changed.connect (bind (&FilmViewer::config_changed, this, _1));
	config_changed (Config::SOUND_OUTPUT);
}

FilmViewer::~FilmViewer ()
{
	stop ();
}

void
FilmViewer::set_film (shared_ptr<Film> film)
{
	if (_film == film) {
		return;
	}

	_film = film;

	FilmChanged ();

	_frame.reset ();
	_closed_captions_dialog->clear ();

	if (!_film) {
		_player.reset ();
		recreate_butler ();
		_frame.reset ();
		refresh_panel ();
		return;
	}

	try {
		_player.reset (new Player (_film, _film->playlist ()));
		_player->set_fast ();
		if (_dcp_decode_reduction) {
			_player->set_dcp_decode_reduction (_dcp_decode_reduction);
		}
	} catch (bad_alloc) {
		error_dialog (_panel, _("There is not enough free memory to do that."));
		_film.reset ();
		return;
	}

	_player->set_always_burn_open_subtitles ();
	_player->set_play_referenced ();

	_film->Change.connect (boost::bind (&FilmViewer::film_change, this, _1, _2));
	_player->Change.connect (boost::bind (&FilmViewer::player_change, this, _1, _2, _3));

	/* Keep about 1 second's worth of history samples */
	_latency_history_count = _film->audio_frame_rate() / _audio_block_size;

	recreate_butler ();

	calculate_sizes ();
	slow_refresh ();
}

void
FilmViewer::recreate_butler ()
{
	bool const was_running = stop ();
	_butler.reset ();

	if (!_film) {
		return;
	}

	AudioMapping map = AudioMapping (_film->audio_channels(), _audio_channels);

	if (_audio_channels != 2 || _film->audio_channels() < 3) {
		for (int i = 0; i < min (_film->audio_channels(), _audio_channels); ++i) {
			map.set (i, i, 1);
		}
	} else {
		/* Special case: stereo output, at least 3 channel input.
		   Map so that Lt = L(-3dB) + Ls(-3dB) + C(-6dB) + Lfe(-10dB)
		               Rt = R(-3dB) + Rs(-3dB) + C(-6dB) + Lfe(-10dB)
		*/
		map.set (dcp::LEFT,   0, 1 / sqrt(2)); // L -> Lt
		map.set (dcp::RIGHT,  1, 1 / sqrt(2)); // R -> Rt
		map.set (dcp::CENTRE, 0, 1 / 2.0); // C -> Lt
		map.set (dcp::CENTRE, 1, 1 / 2.0); // C -> Rt
		map.set (dcp::LFE,    0, 1 / sqrt(10)); // Lfe -> Lt
		map.set (dcp::LFE,    1, 1 / sqrt(10)); // Lfe -> Rt
		map.set (dcp::LS,     0, 1 / sqrt(2)); // Ls -> Lt
		map.set (dcp::RS,     1, 1 / sqrt(2)); // Rs -> Rt
	}

	_butler.reset (new Butler (_player, _film->log(), map, _audio_channels));
	if (!Config::instance()->sound() && !_audio.isStreamOpen()) {
		_butler->disable_audio ();
	}

	_closed_captions_dialog->set_butler (_butler);

	if (was_running) {
		start ();
	}
}

void
FilmViewer::refresh_panel ()
{
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::get ()
{
	DCPOMATIC_ASSERT (_butler);

	do {
		Butler::Error e;
		_player_video = _butler->get_video (&e);
		if (!_player_video.first && e == Butler::AGAIN) {
			signal_manager->when_idle (boost::bind(&FilmViewer::get, this));
			return;
		}
	} while (
		_player_video.first &&
		_film->three_d() &&
		(_eyes != _player_video.first->eyes())
		);

	_butler->rethrow ();

	display_player_video ();
}

void
FilmViewer::display_player_video ()
{
	if (!_player_video.first) {
		_frame.reset ();
		refresh_panel ();
		return;
	}

	if (_playing && (time() - _player_video.second) > one_video_frame()) {
		/* Too late; just drop this frame before we try to get its image (which will be the time-consuming
		   part if this frame is J2K).
		*/
		_video_position = _player_video.second;
		++_dropped;
		return;
	}

	/* In an ideal world, what we would do here is:
	 *
	 * 1. convert to XYZ exactly as we do in the DCP creation path.
	 * 2. convert back to RGB for the preview display, compensating
	 *    for the monitor etc. etc.
	 *
	 * but this is inefficient if the source is RGB.  Since we don't
	 * (currently) care too much about the precise accuracy of the preview's
	 * colour mapping (and we care more about its speed) we try to short-
	 * circuit this "ideal" situation in some cases.
	 *
	 * The content's specified colour conversion indicates the colourspace
	 * which the content is in (according to the user).
	 *
	 * PlayerVideo::image (bound to PlayerVideo::always_rgb) will take the source
	 * image and convert it (from whatever the user has said it is) to RGB.
	 */

	_frame = _player_video.first->image (
		bind (&Log::dcp_log, _film->log().get(), _1, _2),
		bind (&PlayerVideo::always_rgb, _1),
		false, true
		);

	ImageChanged (_player_video.first);

	_video_position = _player_video.second;
	_inter_position = _player_video.first->inter_position ();
	_inter_size = _player_video.first->inter_size ();

	refresh_panel ();

	_closed_captions_dialog->update (time());
}

void
FilmViewer::timer ()
{
	if (!_film || !_playing) {
		return;
	}

	get ();
	PositionChanged ();
	DCPTime const next = _video_position + one_video_frame();

	if (next >= _film->length()) {
		stop ();
	}

	_timer.Start (max ((next.seconds() - time().seconds()) * 1000, 1.0), wxTIMER_ONE_SHOT);

	if (_butler) {
		_butler->rethrow ();
	}
}

void
FilmViewer::paint_panel ()
{
	wxPaintDC dc (_panel);

	if (!_frame || !_film || !_out_size.width || !_out_size.height || _out_size != _frame->size()) {
		dc.Clear ();
		return;
	}

	wxImage frame (_out_size.width, _out_size.height, _frame->data()[0], true);
	wxBitmap frame_bitmap (frame);
	dc.DrawBitmap (frame_bitmap, 0, 0);

	if (_out_size.width < _panel_size.width) {
		wxPen p (_panel->GetParent()->GetBackgroundColour());
		wxBrush b (_panel->GetParent()->GetBackgroundColour());
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (_out_size.width, 0, _panel_size.width - _out_size.width, _panel_size.height);
	}

	if (_out_size.height < _panel_size.height) {
		wxPen p (_panel->GetParent()->GetBackgroundColour());
		wxBrush b (_panel->GetParent()->GetBackgroundColour());
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (0, _out_size.height, _panel_size.width, _panel_size.height - _out_size.height);
	}

	if (_outline_content) {
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (_inter_position.x, _inter_position.y, _inter_size.width, _inter_size.height);
	}
}

void
FilmViewer::set_outline_content (bool o)
{
	_outline_content = o;
	refresh_panel ();
}

void
FilmViewer::set_eyes (Eyes e)
{
	_eyes = e;
	slow_refresh ();
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_size.width = ev.GetSize().GetWidth();
	_panel_size.height = ev.GetSize().GetHeight();

	calculate_sizes ();
	if (!quick_refresh()) {
		slow_refresh ();
	}
	PositionChanged ();
}

void
FilmViewer::calculate_sizes ()
{
	if (!_film || !_player) {
		return;
	}

	Ratio const * container = _film->container ();

	float const panel_ratio = _panel_size.ratio ();
	float const film_ratio = container ? container->ratio () : 1.78;

	if (panel_ratio < film_ratio) {
		/* panel is less widscreen than the film; clamp width */
		_out_size.width = _panel_size.width;
		_out_size.height = lrintf (_out_size.width / film_ratio);
	} else {
		/* panel is more widescreen than the film; clamp height */
		_out_size.height = _panel_size.height;
		_out_size.width = lrintf (_out_size.height * film_ratio);
	}

	/* Catch silly values */
	_out_size.width = max (64, _out_size.width);
	_out_size.height = max (64, _out_size.height);

	_player->set_video_container_size (_out_size);
}

void
FilmViewer::start ()
{
	if (!_film) {
		return;
	}

	if (_audio.isStreamOpen()) {
		_audio.setStreamTime (_video_position.seconds());
		_audio.startStream ();
	}

	_playing = true;
	_dropped = 0;
	timer ();
	Started ();
}

bool
FilmViewer::stop ()
{
	if (_audio.isStreamRunning()) {
		/* stop stream and discard any remaining queued samples */
		_audio.abortStream ();
	}

	if (!_playing) {
		return false;
	}

	_playing = false;
	Stopped ();
	return true;
}

void
FilmViewer::go_to (DCPTime t)
{
	if (t < DCPTime ()) {
		t = DCPTime ();
	}

	if (t >= _film->length ()) {
		t = _film->length ();
	}

	seek (t, true);
	PositionChanged ();
}

void
FilmViewer::player_change (ChangeType type, int property, bool frequent)
{
	if (type != CHANGE_TYPE_DONE || frequent) {
		return;
	}

	if (_coalesce_player_changes) {
		_pending_player_changes.push_back (property);
		return;
	}

	calculate_sizes ();
	bool refreshed = false;
	if (
		property == VideoContentProperty::CROP ||
		property == VideoContentProperty::SCALE ||
		property == VideoContentProperty::FADE_IN ||
		property == VideoContentProperty::FADE_OUT ||
		property == VideoContentProperty::COLOUR_CONVERSION ||
		property == PlayerProperty::VIDEO_CONTAINER_SIZE ||
		property == PlayerProperty::FILM_CONTAINER
		) {
		refreshed = quick_refresh ();
	}

	if (!refreshed) {
		slow_refresh ();
	}
	PositionChanged ();
}

void
FilmViewer::film_change (ChangeType type, Film::Property p)
{
	if (type == CHANGE_TYPE_DONE && p == Film::AUDIO_CHANNELS) {
		recreate_butler ();
	}
}

/** Re-get the current frame slowly by seeking */
void
FilmViewer::slow_refresh ()
{
	seek (_video_position, true);
}

/** Try to re-get the current frame quickly by resetting the metadata
 *  in the PlayerVideo that we used last time.
 *  @return true if this was possible, false if not.
 */
bool
FilmViewer::quick_refresh ()
{
	if (!_player_video.first) {
		return false;
	}

	if (!_player_video.first->reset_metadata (_player->video_container_size(), _film->frame_size())) {
		return false;
	}

	display_player_video ();
	return true;
}

void
FilmViewer::set_position (DCPTime p)
{
	_video_position = p;
	seek (p, true);
	PositionChanged ();
}

void
FilmViewer::set_position (shared_ptr<Content> content, ContentTime t)
{
	optional<DCPTime> dt = _player->content_time_to_dcp (content, t);
	if (dt) {
		set_position (*dt);
	}
}

void
FilmViewer::set_coalesce_player_changes (bool c)
{
	_coalesce_player_changes = c;

	if (!c) {
		BOOST_FOREACH (int i, _pending_player_changes) {
			player_change (CHANGE_TYPE_DONE, i, false);
		}
		_pending_player_changes.clear ();
	}
}

void
FilmViewer::seek (DCPTime t, bool accurate)
{
	if (!_butler) {
		return;
	}

	bool const was_running = stop ();

	_closed_captions_dialog->clear ();
	_butler->seek (t, accurate);
	get ();

	if (was_running) {
		start ();
	}
}

void
FilmViewer::config_changed (Config::Property p)
{
	if (p != Config::SOUND && p != Config::SOUND_OUTPUT) {
		return;
	}

	if (_audio.isStreamOpen ()) {
		_audio.closeStream ();
	}

	if (Config::instance()->sound() && _audio.getDeviceCount() > 0) {
		unsigned int st = 0;
		if (Config::instance()->sound_output()) {
			while (st < _audio.getDeviceCount()) {
				if (_audio.getDeviceInfo(st).name == Config::instance()->sound_output().get()) {
					break;
				}
				++st;
			}
			if (st == _audio.getDeviceCount()) {
				st = _audio.getDefaultOutputDevice();
			}
		} else {
			st = _audio.getDefaultOutputDevice();
		}

		_audio_channels = _audio.getDeviceInfo(st).outputChannels;

		RtAudio::StreamParameters sp;
		sp.deviceId = st;
		sp.nChannels = _audio_channels;
		sp.firstChannel = 0;
		try {
			_audio.openStream (&sp, 0, RTAUDIO_FLOAT32, 48000, &_audio_block_size, &rtaudio_callback, this);
#ifdef DCPOMATIC_USE_RTERROR
		} catch (RtError& e) {
#else
		} catch (RtAudioError& e) {
#endif
			error_dialog (
				_panel,
				_("Could not set up audio output.  There will be no audio during the preview."), std_to_wx(e.what())
				);
		}
		recreate_butler ();

	} else {
		_audio_channels = 0;
		recreate_butler ();
	}
}

DCPTime
FilmViewer::uncorrected_time () const
{
	if (_audio.isStreamRunning ()) {
		return DCPTime::from_seconds (const_cast<RtAudio*>(&_audio)->getStreamTime());
	}

	return _video_position;
}

DCPTime
FilmViewer::time () const
{
	if (_audio.isStreamRunning ()) {
		return DCPTime::from_seconds (const_cast<RtAudio*>(&_audio)->getStreamTime ()) -
			DCPTime::from_frames (average_latency(), _film->audio_frame_rate());
	}

	return _video_position;
}

int
FilmViewer::audio_callback (void* out_p, unsigned int frames)
{
	while (true) {
		optional<DCPTime> t = _butler->get_audio (reinterpret_cast<float*> (out_p), frames);
		if (!t || DCPTime(uncorrected_time() - *t) < one_video_frame()) {
			/* There was an underrun or this audio is on time; carry on */
			break;
		}
		/* The audio we just got was (very) late; drop it and get some more. */
	}

        boost::mutex::scoped_lock lm (_latency_history_mutex, boost::try_to_lock);
        if (lm) {
                _latency_history.push_back (_audio.getStreamLatency ());
                if (_latency_history.size() > static_cast<size_t> (_latency_history_count)) {
                        _latency_history.pop_front ();
                }
        }

	return 0;
}

Frame
FilmViewer::average_latency () const
{
        boost::mutex::scoped_lock lm (_latency_history_mutex);
        if (_latency_history.empty()) {
                return 0;
        }

        Frame total = 0;
        BOOST_FOREACH (Frame i, _latency_history) {
                total += i;
        }

        return total / _latency_history.size();
}

void
FilmViewer::set_dcp_decode_reduction (optional<int> reduction)
{
	_dcp_decode_reduction = reduction;
	if (_player) {
		_player->set_dcp_decode_reduction (reduction);
	}
}

optional<int>
FilmViewer::dcp_decode_reduction () const
{
	return _dcp_decode_reduction;
}

DCPTime
FilmViewer::one_video_frame () const
{
	return DCPTime::from_frames (1, _film->video_frame_rate());
}

void
FilmViewer::show_closed_captions ()
{
	_closed_captions_dialog->Show();
}

void
FilmViewer::move (DCPTime by)
{
	if (!_film) {
		return;
	}

	go_to (_video_position + by);
}

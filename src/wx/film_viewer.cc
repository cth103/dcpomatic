/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "closed_captions_dialog.h"
#include "film_viewer.h"
#include "gl_video_view.h"
#include "nag_dialog.h"
#include "playhead_to_frame_dialog.h"
#include "playhead_to_timecode_dialog.h"
#include "simple_video_view.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/dcpomatic_log.h"
#include "lib/examine_content_job.h"
#include "lib/exceptions.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/image.h"
#include "lib/job_manager.h"
#include "lib/log.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/timer.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include "lib/video_decoder.h"
#include <dcp/exceptions.h>
#include <dcp/warnings.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
LIBDCP_DISABLE_WARNINGS
#include <wx/tglbtn.h>
LIBDCP_ENABLE_WARNINGS
#include <iomanip>


using std::bad_alloc;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::max;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::Size;
using namespace dcpomatic;


static
int
rtaudio_callback (void* out, void *, unsigned int frames, double, RtAudioStreamStatus, void* data)
{
	return reinterpret_cast<FilmViewer*>(data)->audio_callback (out, frames);
}


FilmViewer::FilmViewer (wxWindow* p)
	: _audio (DCPOMATIC_RTAUDIO_API)
	, _closed_captions_dialog (new ClosedCaptionsDialog(p, this))
{
#if wxCHECK_VERSION(3, 1, 0)
	switch (Config::instance()->video_view_type()) {
	case Config::VIDEO_VIEW_OPENGL:
		_video_view = std::make_shared<GLVideoView>(this, p);
		break;
	case Config::VIDEO_VIEW_SIMPLE:
		_video_view = std::make_shared<SimpleVideoView>(this, p);
		break;
	}
#else
	_video_view = std::make_shared<SimpleVideoView>(this, p);
#endif

	_video_view->Sized.connect (boost::bind(&FilmViewer::video_view_sized, this));
	_video_view->TooManyDropped.connect (boost::bind(boost::ref(TooManyDropped)));

	set_film (shared_ptr<Film>());

	_config_changed_connection = Config::instance()->Changed.connect(bind(&FilmViewer::config_changed, this, _1));
	config_changed (Config::SOUND_OUTPUT);
}


FilmViewer::~FilmViewer ()
{
	stop ();
}


/** Ask for ::idle_handler() to be called next time we are idle */
void
FilmViewer::request_idle_display_next_frame ()
{
	if (_idle_get) {
		return;
	}

	_idle_get = true;
	DCPOMATIC_ASSERT (signal_manager);
	signal_manager->when_idle (boost::bind(&FilmViewer::idle_handler, this));
}


void
FilmViewer::idle_handler ()
{
	if (!_idle_get) {
		return;
	}

	if (_video_view->display_next_frame(true) == VideoView::AGAIN) {
		/* get() could not complete quickly so we'll try again later */
		signal_manager->when_idle (boost::bind(&FilmViewer::idle_handler, this));
	} else {
		_idle_get = false;
	}
}


void
FilmViewer::set_film (shared_ptr<Film> film)
{
	if (_film == film) {
		return;
	}

	_film = film;

	_video_view->clear ();
	_closed_captions_dialog->clear ();

	if (!_film) {
		_player = boost::none;
		recreate_butler ();
		_video_view->update ();
		return;
	}

	try {
		_player.emplace(_film, _optimise_for_j2k ? Image::Alignment::COMPACT : Image::Alignment::PADDED);
		_player->set_fast ();
		if (_dcp_decode_reduction) {
			_player->set_dcp_decode_reduction (_dcp_decode_reduction);
		}
	} catch (bad_alloc &) {
		error_dialog (_video_view->get(), _("There is not enough free memory to do that."));
		_film.reset ();
		return;
	}

	_player->set_always_burn_open_subtitles ();
	_player->set_play_referenced ();

	_film->Change.connect (boost::bind (&FilmViewer::film_change, this, _1, _2));
	_film->LengthChange.connect (boost::bind(&FilmViewer::film_length_change, this));
	_player->Change.connect (boost::bind (&FilmViewer::player_change, this, _1, _2, _3));

	film_change (ChangeType::DONE, Film::Property::VIDEO_FRAME_RATE);
	film_change (ChangeType::DONE, Film::Property::THREE_D);
	film_length_change ();

	/* Keep about 1 second's worth of history samples */
	_latency_history_count = _film->audio_frame_rate() / _audio_block_size;

	_closed_captions_dialog->update_tracks (_film);

	recreate_butler ();

	calculate_sizes ();
	slow_refresh ();
}


void
FilmViewer::recreate_butler ()
{
	suspend ();
	_butler.reset ();

	if (!_film) {
		resume ();
		return;
	}

#if wxCHECK_VERSION(3, 1, 0)
	auto const j2k_gl_optimised = dynamic_pointer_cast<GLVideoView>(_video_view) && _optimise_for_j2k;
#else
	auto const j2k_gl_optimised = false;
#endif

	DCPOMATIC_ASSERT(_player);

	_butler = std::make_shared<Butler>(
		_film,
		*_player,
		Config::instance()->audio_mapping(_audio_channels),
		_audio_channels,
		boost::bind(&PlayerVideo::force, AV_PIX_FMT_RGB24),
		VideoRange::FULL,
		j2k_gl_optimised ? Image::Alignment::COMPACT : Image::Alignment::PADDED,
		true,
		j2k_gl_optimised,
		(Config::instance()->sound() && _audio.isStreamOpen()) ? Butler::Audio::ENABLED : Butler::Audio::DISABLED
		);

	_closed_captions_dialog->set_butler (_butler);

	resume ();
}


void
FilmViewer::set_outline_content (bool o)
{
	_outline_content = o;
	_video_view->update ();
}


void
FilmViewer::set_outline_subtitles (optional<dcpomatic::Rect<double>> rect)
{
	_outline_subtitles = rect;
	_video_view->update ();
}


void
FilmViewer::set_eyes (Eyes e)
{
	_video_view->set_eyes (e);
	slow_refresh ();
}


void
FilmViewer::video_view_sized ()
{
	calculate_sizes ();
	if (!quick_refresh()) {
		slow_refresh ();
	}
}


void
FilmViewer::calculate_sizes ()
{
	if (!_film || !_player) {
		return;
	}

	auto const container = _film->container ();

	auto const scale = dpi_scale_factor (_video_view->get());
	int const video_view_width = std::round(_video_view->get()->GetSize().x * scale);
	int const video_view_height = std::round(_video_view->get()->GetSize().y * scale);

	auto const view_ratio = float(video_view_width) / video_view_height;
	auto const film_ratio = container ? container->ratio () : 1.78;

	dcp::Size out_size;
	if (view_ratio < film_ratio) {
		/* panel is less widscreen than the film; clamp width */
		out_size.width = video_view_width;
		out_size.height = lrintf (out_size.width / film_ratio);
	} else {
		/* panel is more widescreen than the film; clamp height */
		out_size.height = video_view_height;
		out_size.width = lrintf (out_size.height * film_ratio);
	}

	/* Catch silly values */
	out_size.width = max (64, out_size.width);
	out_size.height = max (64, out_size.height);

	/* Make sure the video container sizes are always a multiple of 2 so that
	 * we don't get gaps with subsampled sources (e.g. YUV420)
	 */
	if (out_size.width % 2) {
		out_size.width++;
	}
	if (out_size.height % 2) {
		out_size.height++;
	}

	_player->set_video_container_size (out_size);
}


void
FilmViewer::suspend ()
{
	++_suspended;
	if (_audio.isStreamRunning()) {
		_audio.abortStream();
	}
}


void
FilmViewer::start_audio_stream_if_open ()
{
	if (_audio.isStreamOpen()) {
		_audio.setStreamTime (_video_view->position().seconds());
		try {
			_audio.startStream ();
		} catch (RtAudioError& e) {
			_audio_channels = 0;
			error_dialog (
				_video_view->get(),
				_("There was a problem starting audio playback.  Please try another audio output device in Preferences."), std_to_wx(e.what())
				);
		}
	}
}


void
FilmViewer::resume ()
{
	DCPOMATIC_ASSERT (_suspended > 0);
	--_suspended;
	if (_playing && !_suspended) {
		start_audio_stream_if_open ();
		_video_view->start ();
	}
}


void
FilmViewer::start ()
{
	if (!_film) {
		return;
	}

	auto v = PlaybackPermitted ();
	if (v && !*v) {
		/* Computer says no */
		return;
	}

	/* We are about to set up the audio stream from the position of the video view.
	   If there is `lazy' seek in progress we need to wait for it to go through so that
	   _video_view->position() gives us a sensible answer.
	 */
	while (_idle_get) {
		idle_handler ();
	}

	/* Take the video view's idea of position as our `playhead' and start the
	   audio stream (which is the timing reference) there.
         */
	start_audio_stream_if_open ();

	_playing = true;
	/* Calling start() below may directly result in Stopped being emitted, and if that
	 * happens we want it to come after the Started signal, so do that first.
	 */
	Started ();
	_video_view->start ();
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
	_video_view->stop ();
	Stopped ();

	_video_view->rethrow ();
	return true;
}


void
FilmViewer::player_change (ChangeType type, int property, bool frequent)
{
	if (type != ChangeType::DONE || frequent) {
		return;
	}

	if (_coalesce_player_changes) {
		_pending_player_changes.push_back (property);
		return;
	}

	player_change ({property});
}


void
FilmViewer::player_change (vector<int> properties)
{
	calculate_sizes ();

	bool try_quick_refresh = false;
	bool update_ccap_tracks = false;

	for (auto i: properties) {
		if (
			i == VideoContentProperty::CROP ||
			i == VideoContentProperty::CUSTOM_RATIO ||
			i == VideoContentProperty::CUSTOM_SIZE ||
			i == VideoContentProperty::FADE_IN ||
			i == VideoContentProperty::FADE_OUT ||
			i == VideoContentProperty::COLOUR_CONVERSION ||
			i == PlayerProperty::VIDEO_CONTAINER_SIZE ||
			i == PlayerProperty::FILM_CONTAINER
		   ) {
			try_quick_refresh = true;
		}

		if (i == TextContentProperty::USE || i == TextContentProperty::TYPE || i == TextContentProperty::DCP_TRACK) {
			update_ccap_tracks = true;
		}
	}

	if (!try_quick_refresh || !quick_refresh()) {
		slow_refresh ();
	}

	if (update_ccap_tracks) {
		_closed_captions_dialog->update_tracks (_film);
	}
}


void
FilmViewer::film_change (ChangeType type, Film::Property p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (p == Film::Property::AUDIO_CHANNELS) {
		recreate_butler ();
	} else if (p == Film::Property::VIDEO_FRAME_RATE) {
		_video_view->set_video_frame_rate (_film->video_frame_rate());
	} else if (p == Film::Property::THREE_D) {
		_video_view->set_three_d (_film->three_d());
	} else if (p == Film::Property::CONTENT) {
		_closed_captions_dialog->update_tracks (_film);
	}
}


void
FilmViewer::film_length_change ()
{
	_video_view->set_length (_film->length());
}


/** Re-get the current frame slowly by seeking */
void
FilmViewer::slow_refresh ()
{
	seek (_video_view->position(), true);
}


/** Try to re-get the current frame quickly by resetting the metadata
 *  in the PlayerVideo that we used last time.
 *  @return true if this was possible, false if not.
 */
bool
FilmViewer::quick_refresh ()
{
	if (!_video_view || !_film || !_player) {
		return true;
	}
	return _video_view->reset_metadata (_film, _player->video_container_size());
}


void
FilmViewer::seek (shared_ptr<Content> content, ContentTime t, bool accurate)
{
	DCPOMATIC_ASSERT(_player);
	auto dt = _player->content_time_to_dcp (content, t);
	if (dt) {
		seek (*dt, accurate);
	}
}


void
FilmViewer::set_coalesce_player_changes (bool c)
{
	_coalesce_player_changes = c;

	if (!c) {
		player_change (_pending_player_changes);
		_pending_player_changes.clear ();
	}
}


void
FilmViewer::seek (DCPTime t, bool accurate)
{
	if (!_butler) {
		return;
	}

	if (t < DCPTime()) {
		t = DCPTime ();
	}

	if (t >= _film->length()) {
		t = _film->length() - one_video_frame();
	}

	suspend ();

	_closed_captions_dialog->clear ();
	_butler->seek (t, accurate);

	if (!_playing) {
		/* We're not playing, so let the GUI thread get on and
		   come back later to get the next frame after the seek.
		*/
		request_idle_display_next_frame ();
	} else {
		/* We're going to start playing again straight away
		   so wait for the seek to finish.
		*/
		while (_video_view->display_next_frame(false) == VideoView::AGAIN) {}
	}

	resume ();
}


void
FilmViewer::config_changed (Config::Property p)
{
	if (p == Config::AUDIO_MAPPING) {
		recreate_butler ();
		return;
	}

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
				try {
					if (_audio.getDeviceInfo(st).name == Config::instance()->sound_output().get()) {
						break;
					}
				} catch (RtAudioError&) {
					/* Something went wrong with that device so we don't want to use it anyway */
				}
				++st;
			}
			if (st == _audio.getDeviceCount()) {
				st = _audio.getDefaultOutputDevice();
			}
		} else {
			st = _audio.getDefaultOutputDevice();
		}

		try {
			_audio_channels = _audio.getDeviceInfo(st).outputChannels;
			RtAudio::StreamParameters sp;
			sp.deviceId = st;
			sp.nChannels = _audio_channels;
			sp.firstChannel = 0;
			_audio.openStream (&sp, 0, RTAUDIO_FLOAT32, 48000, &_audio_block_size, &rtaudio_callback, this);
		} catch (RtAudioError& e) {
			_audio_channels = 0;
			error_dialog (
				_video_view->get(),
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
	if (_audio.isStreamRunning()) {
		return DCPTime::from_seconds (const_cast<RtAudio*>(&_audio)->getStreamTime());
	}

	return _video_view->position();
}


optional<DCPTime>
FilmViewer::audio_time () const
{
	if (!_audio.isStreamRunning()) {
		return {};
	}

	return DCPTime::from_seconds (const_cast<RtAudio*>(&_audio)->getStreamTime ()) -
		DCPTime::from_frames (average_latency(), _film->audio_frame_rate());
}


DCPTime
FilmViewer::time () const
{
	return audio_time().get_value_or(_video_view->position());
}


int
FilmViewer::audio_callback (void* out_p, unsigned int frames)
{
	while (true) {
		auto t = _butler->get_audio (Butler::Behaviour::NON_BLOCKING, reinterpret_cast<float*> (out_p), frames);
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
        for (auto i: _latency_history) {
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


optional<ContentTime>
FilmViewer::position_in_content (shared_ptr<const Content> content) const
{
	DCPOMATIC_ASSERT(_player);
	return _player->dcp_to_content_time (content, position());
}


DCPTime
FilmViewer::one_video_frame () const
{
	return DCPTime::from_frames (1, _film ? _film->video_frame_rate() : 24);
}


/** Open a dialog box showing our film's closed captions */
void
FilmViewer::show_closed_captions ()
{
	_closed_captions_dialog->Show();
}


void
FilmViewer::seek_by (DCPTime by, bool accurate)
{
	seek (_video_view->position() + by, accurate);
}


void
FilmViewer::set_pad_black (bool p)
{
	_pad_black = p;
}


/** Called when a player has finished the current film.
 *  May be called from a non-UI thread.
 */
void
FilmViewer::finished ()
{
	emit (boost::bind(&FilmViewer::ui_finished, this));
}


/** Called by finished() in the UI thread */
void
FilmViewer::ui_finished ()
{
	stop ();
	Finished ();
}


int
FilmViewer::dropped () const
{
	return _video_view->dropped ();
}


int
FilmViewer::errored () const
{
	return _video_view->errored ();
}


int
FilmViewer::gets () const
{
	return _video_view->gets ();
}


void
FilmViewer::image_changed (shared_ptr<PlayerVideo> pv)
{
	emit (boost::bind(boost::ref(ImageChanged), pv));
}


void
FilmViewer::set_optimise_for_j2k (bool o)
{
	_optimise_for_j2k = o;
	_video_view->set_optimise_for_j2k (o);
}


void
FilmViewer::set_crop_guess (dcpomatic::Rect<float> crop)
{
	if (crop != _crop_guess) {
		_crop_guess = crop;
		_video_view->update ();
	}
}


void
FilmViewer::unset_crop_guess ()
{
	_crop_guess = boost::none;
	_video_view->update ();
}


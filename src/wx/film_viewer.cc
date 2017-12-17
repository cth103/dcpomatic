/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

FilmViewer::FilmViewer (wxWindow* p, bool outline_content, bool jump_to_selected)
	: wxPanel (p)
	, _panel (new wxPanel (this))
	, _outline_content (0)
	, _left_eye (new wxRadioButton (this, wxID_ANY, _("Left eye"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP))
	, _right_eye (new wxRadioButton (this, wxID_ANY, _("Right eye")))
	, _jump_to_selected (0)
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _back_button (new wxButton (this, wxID_ANY, wxT("<")))
	, _forward_button (new wxButton (this, wxID_ANY, wxT(">")))
	, _frame_number (new wxStaticText (this, wxID_ANY, wxT("")))
	, _timecode (new wxStaticText (this, wxID_ANY, wxT("")))
	, _play_button (new wxToggleButton (this, wxID_ANY, _("Play")))
	, _coalesce_player_changes (false)
	, _pending_player_change (false)
	, _last_seek_accurate (true)
	, _audio (DCPOMATIC_RTAUDIO_API)
	, _audio_channels (0)
	, _audio_block_size (1024)
	, _playing (false)
	, _latency_history_count (0)
	, _dropped (0)
{
#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif

	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);

	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	_v_sizer->Add (_panel, 1, wxEXPAND);

	wxBoxSizer* view_options = new wxBoxSizer (wxHORIZONTAL);
	if (outline_content) {
		_outline_content = new wxCheckBox (this, wxID_ANY, _("Outline content"));
		view_options->Add (_outline_content, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
	}
	view_options->Add (_left_eye, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);
	view_options->Add (_right_eye, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);
	if (jump_to_selected) {
		_jump_to_selected = new wxCheckBox (this, wxID_ANY, _("Jump to selected content"));
		view_options->Add (_jump_to_selected, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);
	}
	_v_sizer->Add (view_options, 0, wxALL, DCPOMATIC_SIZER_GAP);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);

	wxBoxSizer* time_sizer = new wxBoxSizer (wxVERTICAL);
	time_sizer->Add (_frame_number, 0, wxEXPAND);
	time_sizer->Add (_timecode, 0, wxEXPAND);

	h_sizer->Add (_back_button, 0, wxALL, 2);
	h_sizer->Add (time_sizer, 0, wxEXPAND);
	h_sizer->Add (_forward_button, 0, wxALL, 2);
	h_sizer->Add (_play_button, 0, wxEXPAND);
	h_sizer->Add (_slider, 1, wxEXPAND);

	_v_sizer->Add (h_sizer, 0, wxEXPAND | wxALL, 6);

	_frame_number->SetMinSize (wxSize (84, -1));
	_back_button->SetMinSize (wxSize (32, -1));
	_forward_button->SetMinSize (wxSize (32, -1));

	_panel->Bind            (wxEVT_PAINT,             boost::bind (&FilmViewer::paint_panel,     this));
	_panel->Bind            (wxEVT_SIZE,              boost::bind (&FilmViewer::panel_sized,     this, _1));
	if (_outline_content) {
		_outline_content->Bind  (wxEVT_CHECKBOX, boost::bind (&FilmViewer::refresh_panel,   this));
	}
	_left_eye->Bind         (wxEVT_RADIOBUTTON,       boost::bind (&FilmViewer::refresh,         this));
	_right_eye->Bind        (wxEVT_RADIOBUTTON,       boost::bind (&FilmViewer::refresh,         this));
	_slider->Bind           (wxEVT_SCROLL_THUMBTRACK, boost::bind (&FilmViewer::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_PAGEUP,     boost::bind (&FilmViewer::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_PAGEDOWN,   boost::bind (&FilmViewer::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_CHANGED,    boost::bind (&FilmViewer::slider_moved,    this, true));
	_play_button->Bind      (wxEVT_TOGGLEBUTTON,      boost::bind (&FilmViewer::play_clicked,    this));
	_timer.Bind             (wxEVT_TIMER,             boost::bind (&FilmViewer::timer,           this));
	_back_button->Bind      (wxEVT_LEFT_DOWN,         boost::bind (&FilmViewer::back_clicked,    this, _1));
	_forward_button->Bind   (wxEVT_LEFT_DOWN,         boost::bind (&FilmViewer::forward_clicked, this, _1));
	_frame_number->Bind     (wxEVT_LEFT_DOWN,         boost::bind (&FilmViewer::frame_number_clicked, this));
	_timecode->Bind         (wxEVT_LEFT_DOWN,         boost::bind (&FilmViewer::timecode_clicked, this));
	if (_jump_to_selected) {
		_jump_to_selected->Bind (wxEVT_CHECKBOX, boost::bind (&FilmViewer::jump_to_selected_clicked, this));
		_jump_to_selected->SetValue (Config::instance()->jump_to_selected ());
	}

	set_film (shared_ptr<Film> ());

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmViewer::active_jobs_changed, this, _2)
		);

	setup_sensitivity ();

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

	_frame.reset ();

	update_position_slider ();
	update_position_label ();

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
		error_dialog (this, _("There is not enough free memory to do that."));
		_film.reset ();
		return;
	}

	/* Always burn in subtitles, even if content is set not to, otherwise we won't see them
	   in the preview.
	*/
	_player->set_always_burn_subtitles (true);
	_player->set_play_referenced ();

	_film->Changed.connect (boost::bind (&FilmViewer::film_changed, this, _1));
	_player->Changed.connect (boost::bind (&FilmViewer::player_changed, this, _1));

	/* Keep about 1 second's worth of history samples */
	_latency_history_count = _film->audio_frame_rate() / _audio_block_size;

	recreate_butler ();

	calculate_sizes ();
	refresh ();

	setup_sensitivity ();
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
		/* Special case: stereo output, at least 3 channel input, map L+R to L/R and
		   C to both, all 3dB down.
		*/
		map.set (0, 0, 1 / sqrt(2)); // L -> L
		map.set (1, 1, 1 / sqrt(2)); // R -> R
		map.set (2, 0, 1 / sqrt(2)); // C -> L
		map.set (2, 1, 1 / sqrt(2)); // C -> R
	}

	_butler.reset (new Butler (_player, _film->log(), map, _audio_channels));
	if (!Config::instance()->sound() && !_audio.isStreamOpen()) {
		_butler->disable_audio ();
	}

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

	pair<shared_ptr<PlayerVideo>, DCPTime> video;
	do {
		video = _butler->get_video ();
	} while (
		_film->three_d() &&
		((_left_eye->GetValue() && video.first->eyes() == EYES_RIGHT) || (_right_eye->GetValue() && video.first->eyes() == EYES_LEFT))
		);

	_butler->rethrow ();

	if (!video.first) {
		_frame.reset ();
		refresh_panel ();
		return;
	}

	if (_playing && (time() - video.second) > one_video_frame()) {
		/* Too late; just drop this frame before we try to get its image (which will be the time-consuming
		   part if this frame is J2K).
		*/
		_video_position = video.second;
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

	_frame = video.first->image (
		bind (&Log::dcp_log, _film->log().get(), _1, _2),
		bind (&PlayerVideo::always_rgb, _1),
		false, true
		);

	ImageChanged (video.first);

	_video_position = video.second;
	_inter_position = video.first->inter_position ();
	_inter_size = video.first->inter_size ();

	refresh_panel ();
}

void
FilmViewer::timer ()
{
	if (!_film || !_playing) {
		return;
	}

	get ();
	update_position_label ();
	update_position_slider ();
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

	if (!_frame || !_film || !_out_size.width || !_out_size.height) {
		dc.Clear ();
		return;
	}

	wxImage frame (_out_size.width, _out_size.height, _frame->data()[0], true);
	wxBitmap frame_bitmap (frame);
	dc.DrawBitmap (frame_bitmap, 0, 0);

	if (_out_size.width < _panel_size.width) {
		wxPen p (GetBackgroundColour ());
		wxBrush b (GetBackgroundColour ());
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (_out_size.width, 0, _panel_size.width - _out_size.width, _panel_size.height);
	}

	if (_out_size.height < _panel_size.height) {
		wxPen p (GetBackgroundColour ());
		wxBrush b (GetBackgroundColour ());
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (0, _out_size.height, _panel_size.width, _panel_size.height - _out_size.height);
	}

	if (_outline_content && _outline_content->GetValue ()) {
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (_inter_position.x, _inter_position.y, _inter_size.width, _inter_size.height);
	}
}

void
FilmViewer::slider_moved (bool update_slider)
{
	if (!_film) {
		return;
	}


	DCPTime t (_slider->GetValue() * _film->length().get() / 4096);
	/* Ensure that we hit the end of the film at the end of the slider */
	if (t >= _film->length ()) {
		t = _film->length() - one_video_frame();
	}
	seek (t, false);
	update_position_label ();
	if (update_slider) {
		update_position_slider ();
	}
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_size.width = ev.GetSize().GetWidth();
	_panel_size.height = ev.GetSize().GetHeight();

	calculate_sizes ();
	refresh ();
	update_position_label ();
	update_position_slider ();
}

void
FilmViewer::calculate_sizes ()
{
	if (!_film) {
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
FilmViewer::play_clicked ()
{
	check_play_state ();
}

void
FilmViewer::check_play_state ()
{
	if (!_film || _film->video_frame_rate() == 0) {
		return;
	}

	if (_play_button->GetValue()) {
		start ();
	} else {
		stop ();
	}
}

void
FilmViewer::start ()
{
	if (_audio.isStreamOpen()) {
		_audio.setStreamTime (_video_position.seconds());
		_audio.startStream ();
	}

	_playing = true;
	_dropped = 0;
	timer ();
}

bool
FilmViewer::stop ()
{
	if (_audio.isStreamRunning()) {
		/* stop stream and discard any remainig queued samples */
		_audio.abortStream ();
	}

	if (!_playing) {
		return false;
	}

	_playing = false;
	_play_button->SetValue (false);
	return true;
}

void
FilmViewer::update_position_slider ()
{
	if (!_film) {
		_slider->SetValue (0);
		return;
	}

	DCPTime const len = _film->length ();

	if (len.get ()) {
		int const new_slider_position = 4096 * _video_position.get() / len.get();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}

void
FilmViewer::update_position_label ()
{
	if (!_film) {
		_frame_number->SetLabel ("0");
		_timecode->SetLabel ("0:0:0.0");
		return;
	}

	double const fps = _film->video_frame_rate ();
	/* Count frame number from 1 ... not sure if this is the best idea */
	_frame_number->SetLabel (wxString::Format (wxT("%ld"), lrint (_video_position.seconds() * fps) + 1));
	_timecode->SetLabel (time_to_timecode (_video_position, fps));
}

void
FilmViewer::active_jobs_changed (optional<string> j)
{
	/* examine content is the only job which stops the viewer working */
	bool const a = !j || *j != "examine_content";
	_slider->Enable (a);
	_play_button->Enable (a);
}

DCPTime
FilmViewer::nudge_amount (wxMouseEvent& ev)
{
	DCPTime amount = one_video_frame ();

	if (ev.ShiftDown() && !ev.ControlDown()) {
		amount = DCPTime::from_seconds (1);
	} else if (!ev.ShiftDown() && ev.ControlDown()) {
		amount = DCPTime::from_seconds (10);
	} else if (ev.ShiftDown() && ev.ControlDown()) {
		amount = DCPTime::from_seconds (60);
	}

	return amount;
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
	update_position_label ();
	update_position_slider ();
}

void
FilmViewer::back_clicked (wxMouseEvent& ev)
{
	go_to (_video_position - nudge_amount (ev));
	ev.Skip ();
}

void
FilmViewer::forward_clicked (wxMouseEvent& ev)
{
	go_to (_video_position + nudge_amount (ev));
	ev.Skip ();
}

void
FilmViewer::player_changed (bool frequent)
{
	if (frequent) {
		return;
	}

	if (_coalesce_player_changes) {
		_pending_player_change = true;
		return;
	}

	calculate_sizes ();
	refresh ();
	update_position_label ();
	update_position_slider ();
}

void
FilmViewer::setup_sensitivity ()
{
	bool const c = _film && !_film->content().empty ();

	_slider->Enable (c);
	_back_button->Enable (c);
	_forward_button->Enable (c);
	_play_button->Enable (c);
	if (_outline_content) {
		_outline_content->Enable (c);
	}
	_frame_number->Enable (c);
	_timecode->Enable (c);
	if (_jump_to_selected) {
		_jump_to_selected->Enable (c);
	}

	_left_eye->Enable (c && _film->three_d ());
	_right_eye->Enable (c && _film->three_d ());
}

void
FilmViewer::film_changed (Film::Property p)
{
	if (p == Film::CONTENT || p == Film::THREE_D) {
		setup_sensitivity ();
	}
}

/** Re-get the current frame */
void
FilmViewer::refresh ()
{
	seek (_video_position, _last_seek_accurate);
}

void
FilmViewer::set_position (DCPTime p)
{
	_video_position = p;
	seek (p, true);
	update_position_label ();
	update_position_slider ();
}

void
FilmViewer::set_coalesce_player_changes (bool c)
{
	_coalesce_player_changes = c;

	if (c) {
		_pending_player_change = false;
	} else {
		if (_pending_player_change) {
			player_changed (false);
		}
	}
}

void
FilmViewer::timecode_clicked ()
{
	PlayheadToTimecodeDialog* dialog = new PlayheadToTimecodeDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		go_to (dialog->get ());
	}
	dialog->Destroy ();
}

void
FilmViewer::frame_number_clicked ()
{
	PlayheadToFrameDialog* dialog = new PlayheadToFrameDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		go_to (dialog->get ());
	}
	dialog->Destroy ();
}

void
FilmViewer::jump_to_selected_clicked ()
{
	Config::instance()->set_jump_to_selected (_jump_to_selected->GetValue ());
}

void
FilmViewer::seek (DCPTime t, bool accurate)
{
	if (!_butler) {
		return;
	}

	bool const was_running = stop ();

	_butler->seek (t, accurate);
	_last_seek_accurate = accurate;
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

	if (Config::instance()->sound()) {
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
				this,
				wxString::Format (_("Could not set up audio output (%s).  There will be no audio during the preview."), e.what())
				);
		}
		recreate_butler ();

	} else {
		_audio_channels = 0;
		recreate_butler ();
	}
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
	_butler->get_audio (reinterpret_cast<float*> (out_p), frames);

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

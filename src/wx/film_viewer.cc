/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/film_viewer.cc
 *  @brief A wx widget to view a preview of a Film.
 */

#include <iostream>
#include <iomanip>
#include <wx/tglbtn.h>
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include "lib/job_manager.h"
#include "lib/image.h"
#include "lib/scaler.h"
#include "lib/exceptions.h"
#include "lib/examine_content_job.h"
#include "lib/filter.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/imagemagick_content.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "video_decoder.h"

using std::string;
using std::pair;
using std::min;
using std::max;
using std::cout;
using std::list;
using std::make_pair;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::weak_ptr;
using libdcp::Size;

FilmViewer::FilmViewer (shared_ptr<Film> f, wxWindow* p)
	: wxPanel (p)
	, _panel (new wxPanel (this))
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _back_button (new wxButton (this, wxID_ANY, wxT("<")))
	, _forward_button (new wxButton (this, wxID_ANY, wxT(">")))
	, _frame_number (new wxStaticText (this, wxID_ANY, wxT("")))
	, _timecode (new wxStaticText (this, wxID_ANY, wxT("")))
	, _play_button (new wxToggleButton (this, wxID_ANY, _("Play")))
	, _got_frame (false)
{
#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif
	
	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
	
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	_v_sizer->Add (_panel, 1, wxEXPAND);

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

	_panel->Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (FilmViewer::paint_panel), 0, this);
	_panel->Connect (wxID_ANY, wxEVT_SIZE, wxSizeEventHandler (FilmViewer::panel_sized), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_PAGEUP, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_play_button->Connect (wxID_ANY, wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler (FilmViewer::play_clicked), 0, this);
	_timer.Connect (wxID_ANY, wxEVT_TIMER, wxTimerEventHandler (FilmViewer::timer), 0, this);
	_back_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmViewer::back_clicked), 0, this);
	_forward_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmViewer::forward_clicked), 0, this);

	set_film (f);

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmViewer::active_jobs_changed, this, _1)
		);
}

void
FilmViewer::set_film (shared_ptr<Film> f)
{
	if (_film == f) {
		return;
	}

	_film = f;

	_frame.reset ();
	_queue.clear ();

	if (!_film) {
		return;
	}

	_player = f->player ();
	_player->disable_audio ();
	_player->Video.connect (boost::bind (&FilmViewer::process_video, this, _1, _3));
	_player->Changed.connect (boost::bind (&FilmViewer::player_changed, this));

	calculate_sizes ();
	fetch_current_frame_again ();
}

void
FilmViewer::fetch_current_frame_again ()
{
	if (!_player) {
		return;
	}
	   
	_player->seek (_player->video_position(), true);
	fetch_next_frame ();
}

void
FilmViewer::timer (wxTimerEvent &)
{
	if (!_player) {
		return;
	}
	
	fetch_next_frame ();

	if (_film->length()) {
		int const new_slider_position = 4096 * _player->video_position() / _film->length();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}


void
FilmViewer::paint_panel (wxPaintEvent &)
{
	wxPaintDC dc (_panel);

	if (!_frame || !_film || !_out_size.width || !_out_size.height) {
		dc.Clear ();
		return;
	}

	shared_ptr<Image> packed_frame (new Image (_frame, false));

	wxImage frame (_out_size.width, _out_size.height, packed_frame->data()[0], true);
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
}


void
FilmViewer::slider_moved (wxScrollEvent &)
{
	if (_film && _player) {
		_player->seek (_slider->GetValue() * _film->length() / 4096, false);
		fetch_next_frame ();
	}
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_size.width = ev.GetSize().GetWidth();
	_panel_size.height = ev.GetSize().GetHeight();
	calculate_sizes ();
	fetch_current_frame_again ();
}

void
FilmViewer::calculate_sizes ()
{
	if (!_film || !_player) {
		return;
	}

	Ratio const * container = _film->container ();
	
	float const panel_ratio = static_cast<float> (_panel_size.width) / _panel_size.height;
	float const film_ratio = container ? container->ratio () : 1.78;
			
	if (panel_ratio < film_ratio) {
		/* panel is less widscreen than the film; clamp width */
		_out_size.width = _panel_size.width;
		_out_size.height = _out_size.width / film_ratio;
	} else {
		/* panel is more widescreen than the film; clamp height */
		_out_size.height = _panel_size.height;
		_out_size.width = _out_size.height * film_ratio;
	}

	/* Catch silly values */
	_out_size.width = max (64, _out_size.width);
	_out_size.height = max (64, _out_size.height);

	_player->set_video_container_size (_out_size);
}

void
FilmViewer::play_clicked (wxCommandEvent &)
{
	check_play_state ();
}

void
FilmViewer::check_play_state ()
{
	if (!_film || _film->dcp_video_frame_rate() == 0) {
		return;
	}
	
	if (_play_button->GetValue()) {
		_timer.Start (1000 / _film->dcp_video_frame_rate());
	} else {
		_timer.Stop ();
	}
}

void
FilmViewer::process_video (shared_ptr<const Image> image, Time t)
{
	if (_got_frame) {
		/* This is an additional frame emitted by a single pass.  Store it. */
		_queue.push_front (make_pair (image, t));
		return;
	}
	
	_frame = image;
	_got_frame = true;

	double const fps = _film->dcp_video_frame_rate ();
	/* Count frame number from 1 ... not sure if this is the best idea */
	_frame_number->SetLabel (wxString::Format (wxT("%d"), int (rint (t * fps / TIME_HZ)) + 1));
	
	double w = static_cast<double>(t) / TIME_HZ;
	int const h = (w / 3600);
	w -= h * 3600;
	int const m = (w / 60);
	w -= m * 60;
	int const s = floor (w);
	w -= s;
	int const f = rint (w * fps);
	_timecode->SetLabel (wxString::Format (wxT("%02d:%02d:%02d:%02d"), h, m, s, f));
}

/** Ask the player to emit its next frame, then update our display */
void
FilmViewer::fetch_next_frame ()
{
	/* Clear our frame in case we don't get a new one */
	_frame.reset ();

	if (!_player) {
		return;
	}

	_got_frame = false;
	
	if (!_queue.empty ()) {
		process_video (_queue.back().first, _queue.back().second);
		_queue.pop_back ();
	} else {
		try {
			while (!_got_frame && !_player->pass ()) {}
		} catch (DecodeError& e) {
			_play_button->SetValue (false);
			check_play_state ();
			error_dialog (this, wxString::Format (_("Could not decode video for view (%s)"), std_to_wx(e.what()).data()));
		}
	}

	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::active_jobs_changed (bool a)
{
	if (a) {
		list<shared_ptr<Job> > jobs = JobManager::instance()->get ();
		list<shared_ptr<Job> >::iterator i = jobs.begin ();		
		while (i != jobs.end() && boost::dynamic_pointer_cast<ExamineContentJob> (*i) == 0) {
			++i;
		}
		
		if (i == jobs.end() || (*i)->finished()) {
			/* no examine content job running, so we're ok to use the viewer */
			a = false;
		}
	}
			
	_slider->Enable (!a);
	_play_button->Enable (!a);
}

void
FilmViewer::back_clicked (wxCommandEvent &)
{
	if (!_player) {
		return;
	}

	_player->seek (_player->video_position() - _film->video_frames_to_time(1), true);
	fetch_next_frame ();
}

void
FilmViewer::forward_clicked (wxCommandEvent &)
{
	if (!_player) {
		return;
	}

	fetch_next_frame ();
}

void
FilmViewer::player_changed ()
{
	calculate_sizes ();
	fetch_current_frame_again ();
}

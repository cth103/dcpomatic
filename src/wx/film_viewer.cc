/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
#include "lib/log.h"
#include "film_viewer.h"
#include "wx_util.h"
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
using dcp::Size;

FilmViewer::FilmViewer (wxWindow* p)
	: wxPanel (p)
	, _panel (new wxPanel (this))
	, _outline_content (new wxCheckBox (this, wxID_ANY, _("Outline content")))
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _back_button (new wxButton (this, wxID_ANY, wxT("<")))
	, _forward_button (new wxButton (this, wxID_ANY, wxT(">")))
	, _frame_number (new wxStaticText (this, wxID_ANY, wxT("")))
	, _timecode (new wxStaticText (this, wxID_ANY, wxT("")))
	, _play_button (new wxToggleButton (this, wxID_ANY, _("Play")))
	, _last_get_accurate (true)
{
#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif
	
	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
	
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	_v_sizer->Add (_panel, 1, wxEXPAND);

	_v_sizer->Add (_outline_content, 0, wxALL, DCPOMATIC_SIZER_GAP);

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

	_panel->Bind          (wxEVT_PAINT,                        boost::bind (&FilmViewer::paint_panel,     this));
	_panel->Bind          (wxEVT_SIZE,                         boost::bind (&FilmViewer::panel_sized,     this, _1));
	_outline_content->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED,     boost::bind (&FilmViewer::refresh_panel,   this));
	_slider->Bind         (wxEVT_SCROLL_THUMBTRACK,            boost::bind (&FilmViewer::slider_moved,    this));
	_slider->Bind         (wxEVT_SCROLL_PAGEUP,                boost::bind (&FilmViewer::slider_moved,    this));
	_slider->Bind         (wxEVT_SCROLL_PAGEDOWN,              boost::bind (&FilmViewer::slider_moved,    this));
	_play_button->Bind    (wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, boost::bind (&FilmViewer::play_clicked,    this));
	_timer.Bind           (wxEVT_TIMER,                        boost::bind (&FilmViewer::timer,           this));
	_back_button->Bind    (wxEVT_COMMAND_BUTTON_CLICKED,       boost::bind (&FilmViewer::back_clicked,    this));
	_forward_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,       boost::bind (&FilmViewer::forward_clicked, this));

	set_film (shared_ptr<Film> ());
	
	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmViewer::active_jobs_changed, this, _1)
		);

	setup_sensitivity ();
}

void
FilmViewer::set_film (shared_ptr<Film> f)
{
	if (_film == f) {
		return;
	}

	_film = f;

	_frame.reset ();
	
	_slider->SetValue (0);
	set_position_text ();
	
	if (!_film) {
		return;
	}

	try {
		_player = f->make_player ();
	} catch (bad_alloc) {
		error_dialog (this, _("There is not enough free memory to do that."));
		_film.reset ();
		return;
	}
	
	_film_connection = _film->Changed.connect (boost::bind (&FilmViewer::film_changed, this, _1));

	_player_connection = _player->Changed.connect (boost::bind (&FilmViewer::player_changed, this, _1));

	calculate_sizes ();
	get (_position, _last_get_accurate);

	setup_sensitivity ();
}

void
FilmViewer::refresh_panel ()
{
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::get (DCPTime p, bool accurate)
{
	if (!_player) {
		return;
	}

	list<shared_ptr<PlayerVideo> > pvf;
	try {
		pvf = _player->get_video (p, accurate);
	} catch (exception& e) {
		error_dialog (this, wxString::Format (_("Could not get video for view (%s)"), std_to_wx(e.what()).data()));
	}
	
	if (!pvf.empty ()) {
		try {
			_frame = pvf.front()->image (PIX_FMT_RGB24, true, boost::bind (&Log::dcp_log, _film->log().get(), _1, _2));

			dcp::YUVToRGB yuv_to_rgb = dcp::YUV_TO_RGB_REC601;
			if (pvf.front()->colour_conversion()) {
				yuv_to_rgb = pvf.front()->colour_conversion().get().yuv_to_rgb();
			}
			
			_frame = _frame->scale (_frame->size(), yuv_to_rgb, PIX_FMT_RGB24, false);
			_position = pvf.front()->time ();
			_inter_position = pvf.front()->inter_position ();
			_inter_size = pvf.front()->inter_size ();
		} catch (dcp::DCPReadError& e) {
			/* This can happen on the following sequence of events:
			 * - load encrypted DCP
			 * - add KDM
			 * - DCP is examined again, which sets its "playable" flag to 1
			 * - as a side effect of the exam, the viewer is updated using the old pieces
			 * - the DCPDecoder in the old piece gives us an encrypted frame
			 * - then, the pieces are re-made (but too late).
			 *
			 * I hope there's a better way to handle this ...
			 */
			_frame.reset ();
			_position = p;
		}
	} else {
		_frame.reset ();
		_position = p;
	}

	set_position_text ();
	refresh_panel ();

	_last_get_accurate = accurate;
}

void
FilmViewer::timer ()
{
	get (_position + DCPTime::from_frames (1, _film->video_frame_rate ()), true);

	DCPTime const len = _film->length ();

	if (len.get ()) {
		int const new_slider_position = 4096 * _position.get() / len.get();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
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

	if (_outline_content->GetValue ()) {
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (_inter_position.x, _inter_position.y, _inter_size.width, _inter_size.height);
	}
}

void
FilmViewer::slider_moved ()
{
	if (!_film) {
		return;
	}

	DCPTime t (_slider->GetValue() * _film->length().get() / 4096);
	/* Ensure that we hit the end of the film at the end of the slider */
	if (t >= _film->length ()) {
		t = _film->length() - DCPTime::from_frames (1, _film->video_frame_rate ());
	}
	get (t, false);
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_size.width = ev.GetSize().GetWidth();
	_panel_size.height = ev.GetSize().GetHeight();

	calculate_sizes ();
	get (_position, _last_get_accurate);
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
		_out_size.height = rint (_out_size.width / film_ratio);
	} else {
		/* panel is more widescreen than the film; clamp height */
		_out_size.height = _panel_size.height;
		_out_size.width = rint (_out_size.height * film_ratio);
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
		_timer.Start (1000 / _film->video_frame_rate());
	} else {
		_timer.Stop ();
	}
}

void
FilmViewer::set_position_text ()
{
	if (!_film) {
		_frame_number->SetLabel ("0");
		_timecode->SetLabel ("0:0:0.0");
		return;
	}
		
	double const fps = _film->video_frame_rate ();
	/* Count frame number from 1 ... not sure if this is the best idea */
	_frame_number->SetLabel (wxString::Format (wxT("%d"), int (rint (_position.seconds() * fps)) + 1));
	_timecode->SetLabel (time_to_timecode (_position, fps));
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
FilmViewer::back_clicked ()
{
	DCPTime p = _position - DCPTime::from_frames (1, _film->video_frame_rate ());
	if (p < DCPTime ()) {
		p = DCPTime ();
	}

	get (p, true);
}

void
FilmViewer::forward_clicked ()
{
	get (_position + DCPTime::from_frames (1, _film->video_frame_rate ()), true);
}

void
FilmViewer::player_changed (bool frequent)
{
	if (frequent) {
		return;
	}

	calculate_sizes ();
	get (_position, _last_get_accurate);
}

void
FilmViewer::setup_sensitivity ()
{
	bool const c = _film && !_film->content().empty ();
	
	_slider->Enable (c);
	_back_button->Enable (c);
	_forward_button->Enable (c);
	_play_button->Enable (c);
	_outline_content->Enable (c);
	_frame_number->Enable (c);
	_timecode->Enable (c);
}

void
FilmViewer::film_changed (Film::Property p)
{
	if (p == Film::CONTENT) {
		setup_sensitivity ();
	}
}

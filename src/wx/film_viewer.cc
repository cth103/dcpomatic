/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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
#include "lib/container.h"
#include "lib/format.h"
#include "lib/util.h"
#include "lib/job_manager.h"
#include "lib/subtitle.h"
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
using std::max;
using std::cout;
using std::list;
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
	, _frame (new wxStaticText (this, wxID_ANY, wxT("")))
	, _timecode (new wxStaticText (this, wxID_ANY, wxT("")))
	, _play_button (new wxToggleButton (this, wxID_ANY, _("Play")))
	, _display_frame_x (0)
	, _got_frame (false)
{
	_panel->SetDoubleBuffered (true);
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
#endif	
	
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	_v_sizer->Add (_panel, 1, wxEXPAND);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);

	wxBoxSizer* time_sizer = new wxBoxSizer (wxVERTICAL);
	time_sizer->Add (_frame, 0, wxEXPAND);
	time_sizer->Add (_timecode, 0, wxEXPAND);

	h_sizer->Add (_back_button, 0, wxALL, 2);
	h_sizer->Add (time_sizer, 0, wxEXPAND);
	h_sizer->Add (_forward_button, 0, wxALL, 2);
	h_sizer->Add (_play_button, 0, wxEXPAND);
	h_sizer->Add (_slider, 1, wxEXPAND);

	_v_sizer->Add (h_sizer, 0, wxEXPAND | wxALL, 6);

	_frame->SetMinSize (wxSize (84, -1));
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
FilmViewer::film_changed (Film::Property p)
{
	switch (p) {
	case Film::CONTAINER:
		calculate_sizes ();
		update_from_raw ();
		break;
	case Film::CONTENT:
	{
		calculate_sizes ();
		wxScrollEvent ev;
		slider_moved (ev);
		break;
	}
	case Film::WITH_SUBTITLES:
	case Film::SUBTITLE_OFFSET:
	case Film::SUBTITLE_SCALE:
		raw_to_display ();
		_panel->Refresh ();
		_panel->Update ();
		break;
	case Film::SCALER:
	case Film::FILTERS:
		update_from_decoder ();
		break;
	default:
		break;
	}
}

void
FilmViewer::set_film (shared_ptr<Film> f)
{
	if (_film == f) {
		return;
	}

	_film = f;

	_raw_frame.reset ();
	_display_frame.reset ();
	_panel->Refresh ();
	_panel->Update ();

	if (!_film) {
		return;
	}

	_player = f->player ();
	_player->disable_audio ();
	/* Don't disable subtitles here as we may need them, and it's nice to be able to turn them
	   on and off without needing obtain a new Player.
	*/
	
	_player->Video.connect (bind (&FilmViewer::process_video, this, _1, _2, _3, _4));
	
	_film->Changed.connect (boost::bind (&FilmViewer::film_changed, this, _1));
	_film->ContentChanged.connect (boost::bind (&FilmViewer::film_content_changed, this, _1, _2));

	film_changed (Film::CONTENT);
	film_changed (Film::CONTAINER);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
}

void
FilmViewer::update_from_decoder ()
{
	if (!_player) {
		return;
	}

	_player->seek (_player->last_video ());
	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::timer (wxTimerEvent &)
{
	if (!_player) {
		return;
	}
	
	_panel->Refresh ();
	_panel->Update ();

	get_frame ();

	if (_film->length()) {
		int const new_slider_position = 4096 * _player->last_video() / _film->length();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}


void
FilmViewer::paint_panel (wxPaintEvent &)
{
	wxPaintDC dc (_panel);

	if (!_display_frame || !_film || !_out_size.width || !_out_size.height) {
		dc.Clear ();
		return;
	}

	if (_display_frame_x) {
		dc.SetPen(*wxBLACK_PEN);
		dc.SetBrush(*wxBLACK_BRUSH);
		dc.DrawRectangle (0, 0, _display_frame_x, _film_size.height);
		dc.DrawRectangle (_display_frame_x + _film_size.width, 0, _display_frame_x, _film_size.height);
	}

	wxImage frame (_film_size.width, _film_size.height, _display_frame->data()[0], true);
	wxBitmap frame_bitmap (frame);
	dc.DrawBitmap (frame_bitmap, _display_frame_x, 0);

	if (_film->with_subtitles() && _display_sub) {
		wxImage sub (_display_sub->size().width, _display_sub->size().height, _display_sub->data()[0], _display_sub->alpha(), true);
		wxBitmap sub_bitmap (sub);
		dc.DrawBitmap (sub_bitmap, _display_sub_position.x, _display_sub_position.y);
	}

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
		_player->seek (_slider->GetValue() * _film->length() / 4096);
	}
	
	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_size.width = ev.GetSize().GetWidth();
	_panel_size.height = ev.GetSize().GetHeight();
	calculate_sizes ();
	update_from_raw ();
}

void
FilmViewer::update_from_raw ()
{
	if (!_raw_frame) {
		return;
	}

	raw_to_display ();
	
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::raw_to_display ()
{
	if (!_raw_frame || _out_size.width < 64 || _out_size.height < 64 || !_film) {
		return;
	}

	shared_ptr<const Image> input = _raw_frame;

	pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
	if (!s.second.empty ()) {
		input = input->post_process (s.second, true);
	}
	
	/* Get a compacted image as we have to feed it to wxWidgets */
	_display_frame = input->scale_and_convert_to_rgb (_film_size, 0, _film->scaler(), false);

	if (_raw_sub) {

		/* Our output is already cropped by the decoder, so we need to account for that
		   when working out the scale that we are applying.
		*/

		/* XXX */
		Size const cropped_size = _raw_frame->size ();//_film->cropped_size (_raw_frame->size ());

		Rect tx = subtitle_transformed_area (
			float (_film_size.width) / cropped_size.width,
			float (_film_size.height) / cropped_size.height,
			_raw_sub->area(), _film->subtitle_offset(), _film->subtitle_scale()
			);
		
		_display_sub.reset (new RGBPlusAlphaImage (_raw_sub->image()->scale (tx.size(), _film->scaler(), false)));
		_display_sub_position = tx.position();
		_display_sub_position.x += _display_frame_x;
	} else {
		_display_sub.reset ();
	}
}	

void
FilmViewer::calculate_sizes ()
{
	if (!_film || !_player) {
		return;
	}

	Container const * container = _film->container ();
	
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

	/* Work out how much padding there is in terms of our display; this will be the x position
	   of our _display_frame.
	*/
	_display_frame_x = 0;
//	if (format) {
//		_display_frame_x = static_cast<float> (format->dcp_padding (_film)) * _out_size.width / format->dcp_size().width;
//	}

	_film_size = _out_size;
	_film_size.width -= _display_frame_x * 2;

	/* Catch silly values */
	if (_out_size.width < 64) {
		_out_size.width = 64;
	}
}

void
FilmViewer::play_clicked (wxCommandEvent &)
{
	check_play_state ();
}

void
FilmViewer::check_play_state ()
{
	if (!_film) {
		return;
	}
	
	if (_play_button->GetValue()) {
		_timer.Start (1000 / _film->dcp_video_frame_rate());
	} else {
		_timer.Stop ();
	}
}

void
FilmViewer::process_video (shared_ptr<const Image> image, bool, shared_ptr<Subtitle> sub, Time t)
{
	_raw_frame = image;
	_raw_sub = sub;

	raw_to_display ();

	_got_frame = true;

	double const fps = _film->dcp_video_frame_rate ();
	_frame->SetLabel (wxString::Format (wxT("%d"), int (rint (t * fps / TIME_HZ))));

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

/** Get a new _raw_frame from the decoder and then do
 *  raw_to_display ().
 */
void
FilmViewer::get_frame ()
{
	/* Clear our raw frame in case we don't get a new one */
	_raw_frame.reset ();

	if (!_player) {
		_display_frame.reset ();
		return;
	}

	try {
		_got_frame = false;
		while (!_got_frame) {
			if (_player->pass ()) {
				/* We didn't get a frame before the decoder gave up,
				   so clear our display frame.
				*/
				_display_frame.reset ();
				break;
			}
		}
	} catch (DecodeError& e) {
		_play_button->SetValue (false);
		check_play_state ();
		error_dialog (this, wxString::Format (_("Could not decode video for view (%s)"), std_to_wx(e.what()).data()));
	}
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
FilmViewer::film_content_changed (weak_ptr<Content>, int p)
{
	if (p == VideoContentProperty::VIDEO_LENGTH) {
		/* Force an update to our frame */
		wxScrollEvent ev;
		slider_moved (ev);
	} else if (p == VideoContentProperty::VIDEO_CROP) {
		update_from_decoder ();
	}		
}

void
FilmViewer::back_clicked (wxCommandEvent &)
{
	if (!_player) {
		return;
	}
	
	_player->seek_back ();
	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::forward_clicked (wxCommandEvent &)
{
	if (!_player) {
		return;
	}

	_player->seek_forward ();
	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

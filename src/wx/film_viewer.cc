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
#include "lib/format.h"
#include "lib/util.h"
#include "lib/job_manager.h"
#include "lib/options.h"
#include "lib/subtitle.h"
#include "lib/image.h"
#include "lib/scaler.h"
#include "lib/exceptions.h"
#include "lib/examine_content_job.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "video_decoder.h"

using std::string;
using std::pair;
using std::max;
using std::cout;
using std::list;
using boost::shared_ptr;

FilmViewer::FilmViewer (shared_ptr<Film> f, wxWindow* p)
	: wxPanel (p)
	, _panel (new wxPanel (this))
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _play_button (new wxToggleButton (this, wxID_ANY, wxT ("Play")))
	, _got_frame (false)
	, _out_width (0)
	, _out_height (0)
	, _panel_width (0)
	, _panel_height (0)
{
	_panel->SetDoubleBuffered (true);
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9	
	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
#endif	
	
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	_v_sizer->Add (_panel, 1, wxEXPAND);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);
	h_sizer->Add (_play_button, 0, wxEXPAND);
	h_sizer->Add (_slider, 1, wxEXPAND);

	_v_sizer->Add (h_sizer, 0, wxEXPAND);

	_panel->Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (FilmViewer::paint_panel), 0, this);
	_panel->Connect (wxID_ANY, wxEVT_SIZE, wxSizeEventHandler (FilmViewer::panel_sized), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_PAGEUP, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_slider->Connect (wxID_ANY, wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler (FilmViewer::slider_moved), 0, this);
	_play_button->Connect (wxID_ANY, wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler (FilmViewer::play_clicked), 0, this);
	_timer.Connect (wxID_ANY, wxEVT_TIMER, wxTimerEventHandler (FilmViewer::timer), 0, this);

	set_film (_film);

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmViewer::active_jobs_changed, this, _1)
		);
}

void
FilmViewer::film_changed (Film::Property p)
{
	switch (p) {
	case Film::FORMAT:
		calculate_sizes ();
		update_from_raw ();
		break;
	case Film::CONTENT:
	{
		shared_ptr<DecodeOptions> o (new DecodeOptions);
		o->decode_audio = false;
		o->decode_subtitles = true;
		o->video_sync = false;
		_decoders = decoder_factory (_film, o, 0);
		_decoders.video->Video.connect (bind (&FilmViewer::process_video, this, _1, _2, _3));
		_decoders.video->OutputChanged.connect (boost::bind (&FilmViewer::decoder_changed, this));
		_decoders.video->set_subtitle_stream (_film->subtitle_stream());
		calculate_sizes ();
		get_frame ();
		_panel->Refresh ();
		_slider->Show (_film->content_type() == VIDEO);
		_play_button->Show (_film->content_type() == VIDEO);
		_v_sizer->Layout ();
		break;
	}
	case Film::WITH_SUBTITLES:
	case Film::SUBTITLE_OFFSET:
	case Film::SUBTITLE_SCALE:
	case Film::SCALER:
		update_from_raw ();
		break;
	case Film::SUBTITLE_STREAM:
		_decoders.video->set_subtitle_stream (_film->subtitle_stream ());
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

	if (!_film) {
		return;
	}

	_film->Changed.connect (boost::bind (&FilmViewer::film_changed, this, _1));

	film_changed (Film::CONTENT);
	film_changed (Film::CROP);
	film_changed (Film::FORMAT);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
	film_changed (Film::SUBTITLE_STREAM);
}

void
FilmViewer::decoder_changed ()
{
	if (_decoders.video->seek_to_last ()) {
		return;
	}

	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::timer (wxTimerEvent& ev)
{
	if (!_film) {
		return;
	}
	
	_panel->Refresh ();
	_panel->Update ();

	get_frame ();

	if (_film->length()) {
		int const new_slider_position = 4096 * _decoders.video->last_source_time() / (_film->length().get() / _film->frames_per_second());
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}


void
FilmViewer::paint_panel (wxPaintEvent& ev)
{
	wxPaintDC dc (_panel);

	if (!_display_frame || !_film || !_out_width || !_out_height) {
		dc.Clear ();
		return;
	}

	wxImage frame (_out_width, _out_height, _display_frame->data()[0], true);
	wxBitmap frame_bitmap (frame);
	dc.DrawBitmap (frame_bitmap, 0, 0);

	if (_film->with_subtitles() && _display_sub) {
		wxImage sub (_display_sub->size().width, _display_sub->size().height, _display_sub->data()[0], _display_sub->alpha(), true);
		wxBitmap sub_bitmap (sub);
		dc.DrawBitmap (sub_bitmap, _display_sub_position.x, _display_sub_position.y);
	}
}


void
FilmViewer::slider_moved (wxScrollEvent &)
{
	if (!_film || !_film->length()) {
		return;
	}
	
	if (_decoders.video->seek (_slider->GetValue() * _film->length().get() / (4096 * _film->frames_per_second()))) {
		return;
	}
	
	get_frame ();
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::panel_sized (wxSizeEvent& ev)
{
	_panel_width = ev.GetSize().GetWidth();
	_panel_height = ev.GetSize().GetHeight();
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
	if (!_raw_frame || _out_width < 64 || _out_height < 64 || !_film) {
		return;
	}

	/* Get a compacted image as we have to feed it to wxWidgets */
	_display_frame = _raw_frame->scale_and_convert_to_rgb (Size (_out_width, _out_height), 0, _film->scaler(), false);

	if (_raw_sub) {
		Rect tx = subtitle_transformed_area (
			float (_out_width) / _film->size().width,
			float (_out_height) / _film->size().height,
			_raw_sub->area(), _film->subtitle_offset(), _film->subtitle_scale()
			);
		
		_display_sub.reset (new RGBPlusAlphaImage (_raw_sub->image()->scale (tx.size(), _film->scaler(), false)));
		_display_sub_position = tx.position();
	} else {
		_display_sub.reset ();
	}
}	

void
FilmViewer::calculate_sizes ()
{
	if (!_film) {
		return;
	}
	
	float const panel_ratio = static_cast<float> (_panel_width) / _panel_height;
	float const film_ratio = _film->format() ? _film->format()->ratio_as_float(_film) : 1.78;
	if (panel_ratio < film_ratio) {
		/* panel is less widscreen than the film; clamp width */
		_out_width = _panel_width;
		_out_height = _out_width / film_ratio;
	} else {
		/* panel is more widescreen than the film; clamp heignt */
		_out_height = _panel_height;
		_out_width = _out_height * film_ratio;
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
		_timer.Start (1000 / _film->frames_per_second());
	} else {
		_timer.Stop ();
	}
}

void
FilmViewer::process_video (shared_ptr<Image> image, bool, shared_ptr<Subtitle> sub)
{
	_raw_frame = image;
	_raw_sub = sub;

	raw_to_display ();

	_got_frame = true;
}

void
FilmViewer::get_frame ()
{
	/* Clear our raw frame in case we don't get a new one */
	_raw_frame.reset ();
	
	try {
		_got_frame = false;
		while (!_got_frame) {
			if (_decoders.video->pass ()) {
				/* We didn't get a frame before the decoder gave up,
				   so clear our display frame.
				*/
				_display_frame.reset ();
				break;
			}
		}
	} catch (DecodeError& e) {
		error_dialog (this, String::compose ("Could not decode video for view (%1)", e.what()));
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


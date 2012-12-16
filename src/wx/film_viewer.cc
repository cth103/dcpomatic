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
#include "film_viewer.h"
#include "wx_util.h"
#include "video_decoder.h"

using std::string;
using std::pair;
using std::max;
using std::cout;
using boost::shared_ptr;

FilmViewer::FilmViewer (shared_ptr<Film> f, wxWindow* p)
	: wxPanel (p)
	, _panel (new wxPanel (this))
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _play_button (new wxToggleButton (this, wxID_ANY, wxT ("Play")))
	, _out_width (0)
	, _out_height (0)
	, _panel_width (0)
	, _panel_height (0)
{
	wxBoxSizer* v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (v_sizer);

	v_sizer->Add (_panel, 1, wxEXPAND);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);
	h_sizer->Add (_play_button, 0, wxEXPAND);
	h_sizer->Add (_slider, 1, wxEXPAND);

	v_sizer->Add (h_sizer, 0, wxEXPAND);

	_panel->Bind (wxEVT_PAINT, &FilmViewer::paint_panel, this);
	_panel->Bind (wxEVT_SIZE, &FilmViewer::panel_sized, this);
	_slider->Bind (wxEVT_SCROLL_THUMBTRACK, &FilmViewer::slider_moved, this);
	_slider->Bind (wxEVT_SCROLL_PAGEUP, &FilmViewer::slider_moved, this);
	_slider->Bind (wxEVT_SCROLL_PAGEDOWN, &FilmViewer::slider_moved, this);
	_play_button->Bind (wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, &FilmViewer::play_clicked, this);
	_timer.Bind (wxEVT_TIMER, &FilmViewer::timer, this);

	set_film (_film);
}

void
FilmViewer::film_changed (Film::Property p)
{
	switch (p) {
	case Film::CROP:
		calculate_sizes ();
		update_from_raw ();
		break;
	case Film::FORMAT:
		calculate_sizes ();
		update_from_raw ();
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

	shared_ptr<DecodeOptions> o (new DecodeOptions);
	o->decode_audio = false;
	o->video_sync = false;
	_decoders = decoder_factory (_film, o, 0);
	_decoders.video->Video.connect (bind (&FilmViewer::process_video, this, _1, _2));
	_decoders.video->OutputChanged.connect (boost::bind (&FilmViewer::decoder_changed, this));

	film_changed (Film::CROP);
	film_changed (Film::FORMAT);
}

void
FilmViewer::decoder_changed ()
{
	shared_ptr<Image> last = _display;
	while (last == _display) {
		_decoders.video->pass ();
	}
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::timer (wxTimerEvent& ev)
{
	_panel->Refresh ();
	_panel->Update ();

	shared_ptr<Image> last = _display;
	while (last == _display) {
		_decoders.video->pass ();
	}

#if 0	
	if (_last_frame_in_seconds) {
		double const video_length_in_seconds = static_cast<double>(_format_context->duration) / AV_TIME_BASE;
		int const new_slider_position = 4096 * _last_frame_in_seconds / video_length_in_seconds;
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
#endif	
}


void
FilmViewer::paint_panel (wxPaintEvent& ev)
{
	wxPaintDC dc (_panel);
	if (!_display) {
		return;
	}

	wxImage i (_out_width, _out_height, _display->data()[0], true);
	wxBitmap b (i);
	dc.DrawBitmap (b, 0, 0);
}


void
FilmViewer::slider_moved (wxCommandEvent& ev)
{
	_decoders.video->seek (_slider->GetValue() * _film->length().get() / 4096);
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
	if (!_raw) {
		return;
	}

	if (_out_width && _out_height) {
		_display = _raw->scale_and_convert_to_rgb (Size (_out_width, _out_height), 0, Scaler::from_id ("bicubic"));
	}
	
	_panel->Refresh ();
	_panel->Update ();
}

void
FilmViewer::calculate_sizes ()
{
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
	if (_play_button->GetValue()) {
		_timer.Start (1000 / _film->frames_per_second());
	} else {
		_timer.Stop ();
	}
}

void
FilmViewer::process_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	_raw = image;
	if (_out_width && _out_height) {
		_display = _raw->scale_and_convert_to_rgb (Size (_out_width, _out_height), 0, Scaler::from_id ("bicubic"));
	}
}

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
		_decoders.video->Video.connect (bind (&FilmViewer::process_video, this, _1, _2));
		_decoders.video->OutputChanged.connect (boost::bind (&FilmViewer::decoder_changed, this));
		_decoders.video->set_subtitle_stream (_film->subtitle_stream());
		calculate_sizes ();
		get_frame ();
		_panel->Refresh ();
		break;
	}
	case Film::WITH_SUBTITLES:
	case Film::SUBTITLE_OFFSET:
	case Film::SUBTITLE_SCALE:
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
	seek_and_update (_decoders.video->last_source_frame ());
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
		int const new_slider_position = 4096 * _decoders.video->last_source_frame() / _film->length().get();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}


void
FilmViewer::paint_panel (wxPaintEvent& ev)
{
	wxPaintDC dc (_panel);

	if (!_display_frame || !_film) {
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
FilmViewer::slider_moved (wxCommandEvent& ev)
{
	if (!_film) {
		return;
	}
	
	if (_film->length()) {
		seek_and_update (_slider->GetValue() * _film->length().get() / 4096);
	}
}

void
FilmViewer::seek_and_update (SourceFrame f)
{
	if (_decoders.video->seek (f)) {
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
	if (!_out_width || !_out_height || !_film) {
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
FilmViewer::process_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	_raw_frame = image;
	_raw_sub = sub;

	raw_to_display ();
}

void
FilmViewer::get_frame ()
{
	if (!_out_width || !_out_height) {
		return;
	}

	shared_ptr<Image> last = _display_frame;
	while (last == _display_frame) {
		if (_decoders.video->pass ()) {
			break;
		}
	}
}

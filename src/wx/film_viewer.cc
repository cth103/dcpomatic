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
 *  @brief A wx widget to view `thumbnails' of a Film.
 */

#include <iostream>
#include <iomanip>
#include "lib/film.h"
#include "lib/format.h"
#include "lib/util.h"
#include "lib/thumbs_job.h"
#include "lib/job_manager.h"
#include "lib/film_state.h"
#include "lib/options.h"
#include "film_viewer.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

class ThumbPanel : public wxPanel
{
public:
	ThumbPanel (wxPanel* parent, Film* film)
		: wxPanel (parent)
		, _film (film)
		, _image (0)
		, _bitmap (0)
	{
	}

	/** Handle a paint event */
	void paint_event (wxPaintEvent& ev)
	{
		if (_current_image != _pending_image) {
			delete _image;
			_image = new wxImage (std_to_wx (_pending_image));
			_current_image = _pending_image;
			setup ();
		}

		if (_current_crop != _pending_crop) {
			_current_crop = _pending_crop;
			setup ();
		}

		wxPaintDC dc (this);
		if (_bitmap) {
			dc.DrawBitmap (*_bitmap, 0, 0, false);
		}
	}

	/** Handle a size event */
	void size_event (wxSizeEvent &)
	{
		if (!_image) {
			return;
		}

		setup ();
		Refresh ();
	}

	void set (string f)
	{
		_pending_image = f;
		Refresh ();
	}

	void set_crop (Crop c)
	{
		_pending_crop = c;
		Refresh ();
	}

	void set_film (Film* f)
	{
		_film = f;
		if (!_film) {
			clear ();
			Refresh ();
		} else {
			setup ();
			Refresh ();
		}
	}

	/** Clear our thumbnail image */
	void clear ()
	{
		delete _bitmap;
		_bitmap = 0;
		delete _image;
		_image = 0;
	}

	void refresh ()
	{
		setup ();
		Refresh ();
	}

	DECLARE_EVENT_TABLE ();

private:

	void setup ()
	{
		if (!_film || !_image) {
			return;
		}
		
		int vw, vh;
		GetSize (&vw, &vh);

		float const target = _film->format() ? _film->format()->ratio_as_float (_film) : 1.78;

		_cropped_image = _image->GetSubImage (
			wxRect (
				_current_crop.left,
				_current_crop.top,
				_image->GetWidth() - (_current_crop.left + _current_crop.right),
				_image->GetHeight() - (_current_crop.top + _current_crop.bottom)
				)
			);

		if ((float (vw) / vh) > target) {
			/* view is longer (horizontally) than the ratio; fit height */
			_cropped_image.Rescale (vh * target, vh, wxIMAGE_QUALITY_HIGH);
		} else {
			/* view is shorter (horizontally) than the ratio; fit width */
			_cropped_image.Rescale (vw, vw / target, wxIMAGE_QUALITY_HIGH);
		}

		delete _bitmap;
		_bitmap = new wxBitmap (_cropped_image);
	}

	Film* _film;
	wxImage* _image;
	std::string _current_image;
	std::string _pending_image;
	wxImage _cropped_image;
	wxBitmap* _bitmap;
	Crop _current_crop;
	Crop _pending_crop;
};

BEGIN_EVENT_TABLE (ThumbPanel, wxPanel)
EVT_PAINT (ThumbPanel::paint_event)
EVT_SIZE (ThumbPanel::size_event)
END_EVENT_TABLE ()

FilmViewer::FilmViewer (Film* f, wxWindow* p)
	: wxPanel (p)
	, _film (0)
{
	_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_sizer);
	
	_thumb_panel = new ThumbPanel (this, f);
	_sizer->Add (_thumb_panel, 1, wxEXPAND);

	int const max = f ? f->num_thumbs() - 1 : 0;
	_slider = new wxSlider (this, wxID_ANY, 0, 0, max);
	_sizer->Add (_slider, 0, wxEXPAND | wxLEFT | wxRIGHT);
	set_thumbnail (0);

	_slider->Connect (wxID_ANY, wxEVT_COMMAND_SLIDER_UPDATED, wxCommandEventHandler (FilmViewer::slider_changed), 0, this);

	set_film (_film);
}

void
FilmViewer::set_thumbnail (int n)
{
	if (_film == 0 || _film->num_thumbs() <= n) {
		return;
	}

	_thumb_panel->set (_film->thumb_file(n));
}

void
FilmViewer::slider_changed (wxCommandEvent &)
{
	set_thumbnail (_slider->GetValue ());
}

void
FilmViewer::film_changed (Film::Property p)
{
	if (p == Film::CROP) {
		_thumb_panel->set_crop (_film->crop ());
	} else if (p == Film::THUMBS) {
		if (_film && _film->num_thumbs() > 1) {
			_slider->SetRange (0, _film->num_thumbs () - 1);
		} else {
			_thumb_panel->clear ();
			_slider->SetRange (0, 1);
		}
		
		_slider->SetValue (0);
		set_thumbnail (0);
	} else if (p == Film::FORMAT) {
		_thumb_panel->refresh ();
	} else if (p == Film::CONTENT) {
		setup_visibility ();
		_film->examine_content ();
		update_thumbs ();
	}
}

void
FilmViewer::set_film (Film* f)
{
	if (_film == f) {
		return;
	}
	
	_film = f;
	_thumb_panel->set_film (_film);

	if (!_film) {
		return;
	}

	_film->Changed.connect (sigc::mem_fun (*this, &FilmViewer::film_changed));
	film_changed (Film::CROP);
	film_changed (Film::THUMBS);
	_thumb_panel->refresh ();
	setup_visibility ();
}

void
FilmViewer::update_thumbs ()
{
	if (!_film) {
		return;
	}

	_film->update_thumbs_pre_gui ();

	shared_ptr<const FilmState> s = _film->state_copy ();
	shared_ptr<Options> o (new Options (s->dir ("thumbs"), ".tiff", ""));
	o->out_size = _film->size ();
	o->apply_crop = false;
	o->decode_audio = false;
	o->decode_video_frequency = 128;
	
	shared_ptr<Job> j (new ThumbsJob (s, o, _film->log ()));
	j->Finished.connect (sigc::mem_fun (_film, &Film::update_thumbs_post_gui));
	JobManager::instance()->add (j);
}

void
FilmViewer::setup_visibility ()
{
	if (!_film) {
		return;
	}

	ContentType const c = _film->content_type ();
	_slider->Show (c == VIDEO);
}

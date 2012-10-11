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
		, _frame_rebuild_needed (false)
		, _composition_needed (false)
	{}

	/** Handle a paint event */
	void paint_event (wxPaintEvent& ev)
	{
		if (_frame_rebuild_needed) {
			_image.reset (new wxImage (std_to_wx (_film->thumb_file (_index))));

			_subtitles.clear ();
			list<pair<Position, string> > s = _film->thumb_subtitles (_index);
			for (list<pair<Position, string> >::iterator i = s.begin(); i != s.end(); ++i) {
				_subtitles.push_back (SubtitleView (i->first, std_to_wx (i->second)));
			}

			_frame_rebuild_needed = false;

			compose ();
			_composition_needed = false;
		}

		if (_composition_needed) {
			compose ();
			_composition_needed = false;
		}

		wxPaintDC dc (this);
		if (_bitmap) {
			dc.DrawBitmap (*_bitmap, 0, 0, false);
		}

		if (_film && _film->with_subtitles ()) {
			for (list<SubtitleView>::iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
				dc.DrawBitmap (*i->bitmap, i->cropped_position.x, i->cropped_position.y, true);
			}
		}
	}

	/** Handle a size event */
	void size_event (wxSizeEvent &)
	{
		if (!_image) {
			return;
		}

		recompose ();
	}

	/** @param n Thumbnail index */
	void set (int n)
	{
		_index = n;
		_frame_rebuild_needed = true;
		Refresh ();
	}

	void set_film (Film* f)
	{
		_film = f;
		if (!_film) {
			clear ();
			_frame_rebuild_needed = true;
			Refresh ();
		} else {
			_frame_rebuild_needed = true;
			Refresh ();
		}
	}

	/** Clear our thumbnail image */
	void clear ()
	{
		_bitmap.reset ();
		_image.reset ();
		_subtitles.clear ();
	}

	void recompose ()
	{
		_composition_needed = true;
		Refresh ();
	}

	DECLARE_EVENT_TABLE ();

private:

	void compose ()
	{
		if (!_film || !_image) {
			return;
		}

		/* Size of the view */
		int vw, vh;
		GetSize (&vw, &vh);

		/* Cropped rectangle */
		Rectangle cropped (
			_film->crop().left,
			_film->crop().top,
			_image->GetWidth() - (_film->crop().left + _film->crop().right),
			_image->GetHeight() - (_film->crop().top + _film->crop().bottom)
			);

		/* Target ratio */
		float const target = _film->format() ? _film->format()->ratio_as_float (_film) : 1.78;

		_cropped_image = _image->GetSubImage (wxRect (cropped.x, cropped.y, cropped.w, cropped.h));

		float x_scale = 1;
		float y_scale = 1;

		if ((float (vw) / vh) > target) {
			/* view is longer (horizontally) than the ratio; fit height */
			_cropped_image.Rescale (vh * target, vh, wxIMAGE_QUALITY_HIGH);
			x_scale = vh * target / cropped.w;
			y_scale = float (vh) / cropped.h;
		} else {
			/* view is shorter (horizontally) than the ratio; fit width */
			_cropped_image.Rescale (vw, vw / target, wxIMAGE_QUALITY_HIGH);
			x_scale = float (vw) / cropped.w;
			y_scale = (vw / target) / cropped.h;
		}

		_bitmap.reset (new wxBitmap (_cropped_image));

		for (list<SubtitleView>::iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {

			/* Area of the subtitle graphic within the (uncropped) picture frame */
			Rectangle sub_rect (i->position.x, i->position.y + _film->subtitle_offset(), i->image.GetWidth(), i->image.GetHeight());
			/* Hence the subtitle graphic after it has been cropped */
			Rectangle cropped_sub_rect = sub_rect.intersection (cropped);

			/* Get the cropped version of the subtitle image */
			i->cropped_image = i->image.GetSubImage (
				wxRect (
					cropped_sub_rect.x - sub_rect.x,
					cropped_sub_rect.y - sub_rect.y,
					cropped_sub_rect.w,
					cropped_sub_rect.h
					)
				);

			i->cropped_image.Rescale (cropped_sub_rect.w * x_scale, cropped_sub_rect.h * y_scale, wxIMAGE_QUALITY_HIGH);

			i->cropped_position = Position (
				cropped_sub_rect.x * x_scale,
				(cropped_sub_rect.y - _film->crop().top) * y_scale
				);

			i->bitmap.reset (new wxBitmap (i->cropped_image));
		}
	}

	Film* _film;
	shared_ptr<wxImage> _image;
	wxImage _cropped_image;
	/** currently-displayed thumbnail index */
	int _index;
	shared_ptr<wxBitmap> _bitmap;
	bool _frame_rebuild_needed;
	bool _composition_needed;

	struct SubtitleView
	{
		SubtitleView (Position p, wxString const & i)
			: position (p)
			, image (i)
		{}
			      
		Position position;
		wxImage image;
		Position cropped_position;
		wxImage cropped_image;
		shared_ptr<wxBitmap> bitmap;
	};

	list<SubtitleView> _subtitles;
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

	_thumb_panel->set (n);
}

void
FilmViewer::slider_changed (wxCommandEvent &)
{
	set_thumbnail (_slider->GetValue ());
}

void
FilmViewer::film_changed (Film::Property p)
{
	switch (p) {
	case Film::THUMBS:
		if (_film && _film->num_thumbs() > 1) {
			_slider->SetRange (0, _film->num_thumbs () - 1);
		} else {
			_thumb_panel->clear ();
			_slider->SetRange (0, 1);
		}
		
		_slider->SetValue (0);
		set_thumbnail (0);
		break;
	case Film::CONTENT:
		setup_visibility ();
		_film->examine_content ();
		update_thumbs ();
		break;
	case Film::CROP:
	case Film::FORMAT:
	case Film::WITH_SUBTITLES:
	case Film::SUBTITLE_OFFSET:
	case Film::SUBTITLE_SCALE:
		_thumb_panel->recompose ();
		break;
	default:
		break;
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
	shared_ptr<Options> o (new Options (s->dir ("thumbs"), ".png", ""));
	o->out_size = _film->size ();
	o->apply_crop = false;
	o->decode_audio = false;
	o->decode_video_frequency = 128;
	
	shared_ptr<Job> j (new ThumbsJob (s, o, _film->log(), shared_ptr<Job> ()));
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

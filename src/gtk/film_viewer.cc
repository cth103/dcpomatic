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
 *  @brief A GTK widget to view `thumbnails' of a Film.
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

using namespace std;
using namespace boost;

FilmViewer::FilmViewer (Film* f)
	: _film (f)
	, _update_button ("Update")
{
	_scroller.add (_image);

	Gtk::HBox* controls = manage (new Gtk::HBox);
	controls->set_spacing (6);
	controls->pack_start (_update_button, false, false);
	controls->pack_start (_position_slider);
	
	_vbox.pack_start (_scroller, true, true);
	_vbox.pack_start (*controls, false, false);
	_vbox.set_border_width (12);

	_update_button.signal_clicked().connect (sigc::mem_fun (*this, &FilmViewer::update_thumbs));

	_position_slider.set_digits (0);
	_position_slider.signal_format_value().connect (sigc::mem_fun (*this, &FilmViewer::format_position_slider_value));
	_position_slider.signal_value_changed().connect (sigc::mem_fun (*this, &FilmViewer::position_slider_changed));

	_scroller.signal_size_allocate().connect (sigc::mem_fun (*this, &FilmViewer::scroller_size_allocate));

	set_film (_film);
}

void
FilmViewer::load_thumbnail (int n)
{
	if (_film == 0 || _film->num_thumbs() <= n) {
		return;
	}

	int const left = _film->left_crop ();
	int const right = _film->right_crop ();
	int const top = _film->top_crop ();
	int const bottom = _film->bottom_crop ();

	_pixbuf = Gdk::Pixbuf::create_from_file (_film->thumb_file (n));

	int const cw = _film->size().width - left - right;
	int const ch = _film->size().height - top - bottom;
	_cropped_pixbuf = Gdk::Pixbuf::create_subpixbuf (_pixbuf, left, top, cw, ch);
	update_scaled_pixbuf ();
	_image.set (_scaled_pixbuf);
}

void
FilmViewer::reload_current_thumbnail ()
{
	load_thumbnail (_position_slider.get_value ());
}

void
FilmViewer::position_slider_changed ()
{
	reload_current_thumbnail ();
}

string
FilmViewer::format_position_slider_value (double v) const
{
	stringstream s;

	if (_film && int (v) < _film->num_thumbs ()) {
		int const f = _film->thumb_frame (int (v));
		s << f << " " << seconds_to_hms (f / _film->frames_per_second ());
	} else {
		s << "-";
	}
	
	return s.str ();
}

void
FilmViewer::film_changed (Film::Property p)
{
	if (p == Film::LEFT_CROP || p == Film::RIGHT_CROP || p == Film::TOP_CROP || p == Film::BOTTOM_CROP) {
		reload_current_thumbnail ();
	} else if (p == Film::THUMBS) {
		if (_film && _film->num_thumbs() > 1) {
			_position_slider.set_range (0, _film->num_thumbs () - 1);
		} else {
			_image.clear ();
			_position_slider.set_range (0, 1);
		}
		
		_position_slider.set_value (0);
		reload_current_thumbnail ();
	} else if (p == Film::FORMAT) {
		reload_current_thumbnail ();
	} else if (p == Film::CONTENT) {
		setup_visibility ();
		_film->examine_content ();
		update_thumbs ();
	}
}

void
FilmViewer::set_film (Film* f)
{
	_film = f;

	_update_button.set_sensitive (_film != 0);
	
	if (!_film) {
		_image.clear ();
		return;
	}

	_film->Changed.connect (sigc::mem_fun (*this, &FilmViewer::film_changed));

	film_changed (Film::THUMBS);
}

pair<int, int>
FilmViewer::scaled_pixbuf_size () const
{
	if (_film == 0) {
		return make_pair (0, 0);
	}
	
	int const cw = _film->size().width - _film->left_crop() - _film->right_crop(); 
	int const ch = _film->size().height - _film->top_crop() - _film->bottom_crop();

	float ratio = 1;
	if (_film->format()) {
		ratio = _film->format()->ratio_as_float() * ch / cw;
	}

	Gtk::Allocation const a = _scroller.get_allocation ();
	float const zoom = min (float (a.get_width()) / (cw * ratio), float (a.get_height()) / cw);
	return make_pair (cw * zoom * ratio, ch * zoom);
}
	
void
FilmViewer::update_scaled_pixbuf ()
{
	pair<int, int> const s = scaled_pixbuf_size ();

	if (s.first > 0 && s.second > 0 && _cropped_pixbuf) {
		_scaled_pixbuf = _cropped_pixbuf->scale_simple (s.first, s.second, Gdk::INTERP_HYPER);
		_image.set (_scaled_pixbuf);
	}
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
FilmViewer::scroller_size_allocate (Gtk::Allocation a)
{
	if (a.get_width() != _last_scroller_allocation.get_width() || a.get_height() != _last_scroller_allocation.get_height()) {
		update_scaled_pixbuf ();
	}
	
	_last_scroller_allocation = a;
}

void
FilmViewer::setup_visibility ()
{
	if (!_film) {
		return;
	}

	ContentType const c = _film->content_type ();
	_update_button.property_visible() = (c == VIDEO);
	_position_slider.property_visible() = (c == VIDEO);
}

/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <wx/wx.h>
#include "util.h"

class Film;
class View;
class ContentView;

class Timeline : public wxPanel
{
public:
	Timeline (wxWindow *, boost::shared_ptr<const Film>);

	boost::shared_ptr<const Film> film () const;

	void force_redraw (Rect const &);

	int x_offset () const {
		return 8;
	}

	int width () const {
		return GetSize().GetWidth ();
	}

	int track_height () const {
		return 64;
	}

	double pixels_per_time_unit () const {
		return _pixels_per_time_unit;
	}

	Position tracks_position () const {
		return Position (8, 8);
	}

	int tracks () const;

private:
	void paint (wxPaintEvent &);
	void left_down (wxMouseEvent &);
	void mouse_moved (wxMouseEvent &);
	void left_up (wxMouseEvent &);
	void playlist_changed ();
	void setup_pixels_per_time_unit ();
	void resized (wxSizeEvent &);

	boost::weak_ptr<const Film> _film;
	std::list<boost::shared_ptr<View> > _views;
	double _pixels_per_time_unit;
	bool _left_down;
	wxPoint _down_point;
	boost::shared_ptr<ContentView> _down_view;
	Time _down_view_start;
	bool _first_move;
};

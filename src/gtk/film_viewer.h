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

/** @file  src/film_viewer.h
 *  @brief A GTK widget to view `thumbnails' of a Film.
 */

#include <gtkmm.h>
#include "lib/film.h"

/** @class FilmViewer
 *  @brief A GTK widget to view `thumbnails' of a Film.
 */
class FilmViewer
{
public:
	FilmViewer (Film *);

	Gtk::Widget& widget () {
		return _vbox;
	}

	void set_film (Film *);
	void setup_visibility ();

private:
	void position_slider_changed ();
	void update_thumbs ();
	std::string format_position_slider_value (double) const;
	void load_thumbnail (int);
	void film_changed (Film::Property);
	void reload_current_thumbnail ();
	void update_scaled_pixbuf ();
	std::pair<int, int> scaled_pixbuf_size () const;
	void scroller_size_allocate (Gtk::Allocation);

	Film* _film;
	Gtk::VBox _vbox;
	Gtk::ScrolledWindow _scroller;
	Gtk::Image _image;
	Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
	Glib::RefPtr<Gdk::Pixbuf> _cropped_pixbuf;
	Glib::RefPtr<Gdk::Pixbuf> _scaled_pixbuf;
	Gtk::HScale _position_slider;
	Gtk::Button _update_button;
	Gtk::Allocation _last_scroller_allocation;
};

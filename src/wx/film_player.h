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

#include <gtkmm.h>
#include "lib/film.h"

class Film;
class Screen;
class FilmState;

class FilmPlayer
{
public:
	FilmPlayer (Film const * f = 0);
	
	Gtk::Widget& widget ();
	
	void set_film (Film const *);
	void setup_visibility ();

private:
	void play_clicked ();
	void pause_clicked ();
	void stop_clicked ();
	void position_changed ();
	std::string format_position (double);
	void film_changed (Film::Property);
	
	void set_button_states ();
	boost::shared_ptr<Screen> screen () const;
	void set_status ();
	void update ();
	void update_screens ();
	
	Film const * _film;
	boost::shared_ptr<const FilmState> _last_play_fs;

	Gtk::VBox _main_vbox;
	Gtk::Button _play;
	Gtk::Button _pause;
	Gtk::Button _stop;
	Gtk::Label _status;
	Gtk::CheckButton _ab;
	Gtk::ComboBoxText _screen;
	Gtk::HScale _position;
	bool _ignore_position_changed;
};

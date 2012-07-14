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
#include "lib/trim_action.h"

class Film;

class DCPRangeDialog : public Gtk::Dialog
{
public:
	DCPRangeDialog (Film *);

	sigc::signal2<void, int, TrimAction> Changed;

private:
	void whole_toggled ();
	void cut_toggled ();
	void n_frames_changed ();
	
	void set_sensitivity ();
	void emit_changed ();
	
	Film* _film;
	Gtk::RadioButton _whole;
	Gtk::RadioButton _first;
	Gtk::SpinButton _n_frames;
	Gtk::RadioButton _cut;
	Gtk::RadioButton _black_out;
};

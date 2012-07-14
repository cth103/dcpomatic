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

#include "dcp_range_dialog.h"
#include "lib/film.h"

DCPRangeDialog::DCPRangeDialog (Film* f)
	: _film (f)
	, _whole ("Whole film")
	, _first ("First")
	, _cut ("Cut remainder")
	, _black_out ("Black-out remainder")
{
	set_title ("DCP range");

	Gtk::Table* table = Gtk::manage (new Gtk::Table ());
	table->set_border_width (6);
	table->set_row_spacings (6);
	table->set_col_spacings (6);
	table->attach (_whole, 0, 4, 0, 1);
	table->attach (_first, 0, 1, 1, 2);
	table->attach (_n_frames, 1, 2, 1, 2);
	table->attach (*manage (new Gtk::Label ("frames")), 2, 3, 1, 2);
	table->attach (_cut, 1, 2, 2, 3);
	table->attach (_black_out, 1, 2, 3, 4);

	Gtk::RadioButtonGroup g = _whole.get_group ();
	_first.set_group (g);

	g = _black_out.get_group ();
	_cut.set_group (g);

	_n_frames.set_range (1, INT_MAX - 1);
	_n_frames.set_increments (24, 24 * 60);
	if (_film->dcp_frames() > 0) {
		_whole.set_active (false);
		_first.set_active (true);
		_n_frames.set_value (_film->dcp_frames ());
	} else {
		_whole.set_active (true);
		_first.set_active (false);
		_n_frames.set_value (24);
	}

	_black_out.set_active (_film->dcp_trim_action() == BLACK_OUT);
	_cut.set_active (_film->dcp_trim_action() == CUT);

	_whole.signal_toggled().connect (sigc::mem_fun (*this, &DCPRangeDialog::whole_toggled));
	_cut.signal_toggled().connect (sigc::mem_fun (*this, &DCPRangeDialog::cut_toggled));
	_n_frames.signal_value_changed().connect (sigc::mem_fun (*this, &DCPRangeDialog::n_frames_changed));

	get_vbox()->pack_start (*table);

	add_button ("Close", Gtk::RESPONSE_CLOSE);
	show_all_children ();

	set_sensitivity ();
}

void
DCPRangeDialog::whole_toggled ()
{
	set_sensitivity ();
	emit_changed ();
}

void
DCPRangeDialog::set_sensitivity ()
{
	_n_frames.set_sensitive (_first.get_active ());
	_black_out.set_sensitive (_first.get_active ());
	_cut.set_sensitive (_first.get_active ());
}

void
DCPRangeDialog::cut_toggled ()
{
	emit_changed ();
}

void
DCPRangeDialog::n_frames_changed ()
{
	emit_changed ();
}

void
DCPRangeDialog::emit_changed ()
{
	int frames = 0;
	if (!_whole.get_active ()) {
		frames = _n_frames.get_value_as_int ();
	}

	TrimAction action = CUT;
	if (_black_out.get_active ()) {
		action = BLACK_OUT;
	}

	Changed (frames, action);
}

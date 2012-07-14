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

/** @file  src/filter_dialog.cc
 *  @brief A dialog to select FFmpeg filters.
 */

#include "lib/film.h"
#include "filter_dialog.h"

using namespace std;

FilterDialog::FilterDialog (vector<Filter const *> const & f)
	: Gtk::Dialog ("Filters")
	, _filters (f)
{
	get_vbox()->pack_start (_filters.widget ());

	_filters.ActiveChanged.connect (sigc::mem_fun (*this, &FilterDialog::active_changed));

	add_button ("Close", Gtk::RESPONSE_CLOSE);

	show_all ();
}
	

void
FilterDialog::active_changed ()
{
	ActiveChanged (_filters.active ());
}

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

/** @file src/gtk/util.cc
 *  @brief Some utility functions.
 */

#include <gtkmm.h>

using namespace std;

/** @param t Label text.
 *  @return GTK label containing t, left-aligned (passed through Gtk::manage)
 */
Gtk::Label &
left_aligned_label (string t)
{
	Gtk::Label* l = Gtk::manage (new Gtk::Label (t));
	l->set_alignment (0, 0.5);
	return *l;
}

void
error_dialog (string m)
{
	Gtk::MessageDialog d (m, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
	d.set_title ("DVD-o-matic");
	d.run ();
}

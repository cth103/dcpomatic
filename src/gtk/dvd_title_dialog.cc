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

#include <cassert>
#include <iostream>
#include "lib/exceptions.h"
#include "dvd_title_dialog.h"
#include "gtk_util.h"

using namespace std;

DVDTitleDialog::DVDTitleDialog ()
{
	string const dvd = find_dvd ();
	if (dvd.empty ()) {
		throw DVDError ("could not find DVD");
	}

	list<DVDTitle> t = dvd_titles (dvd);
	if (t.empty ()) {
		throw DVDError ("no titles found on DVD");
	}

	set_title ("Choose DVD title");
	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (3);

	Gtk::RadioButtonGroup g;
	for (list<DVDTitle>::const_iterator i = t.begin(); i != t.end(); ++i) {
		Gtk::RadioButton* b = manage (new Gtk::RadioButton);
		stringstream s;
		s << "Title " << i->number << ": " << g_format_size (i->size);
		b->set_label (s.str ());
		if (i == t.begin ()) {
			b->set_active ();
			g = b->get_group ();
		} else {
			b->set_group (g);
		}
		get_vbox()->pack_start (*b);
		_buttons[*i] = b;
	}

	add_button ("Cancel", Gtk::RESPONSE_CANCEL);
	add_button ("Copy Title", Gtk::RESPONSE_OK);

	show_all ();
}

DVDTitle
DVDTitleDialog::selected ()
{
	std::map<DVDTitle, Gtk::RadioButton *>::const_iterator i = _buttons.begin ();
	while (i != _buttons.end() && i->second->get_active() == false) {
		++i;
	}

	assert (i != _buttons.end ());
	return i->first;
}

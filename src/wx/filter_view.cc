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

/** @file  src/filter_view.cc
 *  @brief A widget to select FFmpeg filters.
 */

#include <iostream>
#include "lib/filter.h"
#include "filter_view.h"

using namespace std;

FilterView::FilterView (vector<Filter const *> const & active)
{
	_box.set_spacing (4);
	
	vector<Filter const *> filters = Filter::all ();

	for (vector<Filter const *>::iterator i = filters.begin(); i != filters.end(); ++i) {
		Gtk::CheckButton* b = Gtk::manage (new Gtk::CheckButton ((*i)->name()));
		bool const a = find (active.begin(), active.end(), *i) != active.end ();
		b->set_active (a);
		_filters[*i] = a;
		b->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &FilterView::filter_toggled), *i));
		_box.pack_start (*b, false, false);
	}

	_box.show_all ();
}

Gtk::Widget &
FilterView::widget ()
{
	return _box;
}

void
FilterView::filter_toggled (Filter const * f)
{
	_filters[f] = !_filters[f];
	ActiveChanged ();
}

vector<Filter const*>
FilterView::active () const
{
	vector<Filter const *> active;
	for (map<Filter const *, bool>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		if (i->second) {
			active.push_back (i->first);
		}
	}

	return active;
}

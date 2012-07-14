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

#include <boost/filesystem.hpp>
#include "lib/film.h"
#include "film_list.h"

using namespace std;
using namespace boost;

FilmList::FilmList (string d)
	: _directory (d)
	, _list (1)
{
	for (filesystem::directory_iterator i = filesystem::directory_iterator (_directory); i != filesystem::directory_iterator(); ++i) {
		if (is_directory (*i)) {
			filesystem::path m = filesystem::path (*i) / filesystem::path ("metadata");
			if (is_regular_file (m)) {
				Film* f = new Film (i->path().string());
				_films.push_back (f);
			}
		}
	}

	for (vector<Film const *>::iterator i = _films.begin(); i != _films.end(); ++i) {
		_list.append_text ((*i)->name ());
	}

	_list.set_headers_visible (false);
	_list.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &FilmList::selection_changed));
}

Gtk::Widget&
FilmList::widget ()
{
	return _list;
}

void
FilmList::selection_changed ()
{
	Gtk::ListViewText::SelectionList s = _list.get_selected ();
	if (s.empty ()) {
		return;
	}

	assert (s[0] < int (_films.size ()));
	SelectionChanged (_films[s[0]]);
}

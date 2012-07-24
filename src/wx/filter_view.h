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

/** @file  src/filter_view.h
 *  @brief A widget to select FFmpeg filters.
 */

#include <gtkmm.h>
#include <vector>

class Filter;

/** @class FilterView
 *  @brief A widget to select FFmpeg filters.
 */
class FilterView
{
public:
	FilterView (std::vector<Filter const *> const &);

	Gtk::Widget & widget ();
	std::vector<Filter const *> active () const;

	sigc::signal0<void> ActiveChanged;

private:
	void filter_toggled (Filter const *);

	Gtk::VBox _box;
	std::map<Filter const *, bool> _filters;
};

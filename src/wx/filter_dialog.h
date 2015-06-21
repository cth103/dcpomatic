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

/** @file  src/filter_dialog.h
 *  @brief A dialog to select FFmpeg filters.
 */

#include <wx/wx.h>
#include <boost/signals2.hpp>

class Film;
class FilterEditor;
class Filter;

/** @class FilterDialog
 *  @brief A dialog to select FFmpeg filters.
 */
class FilterDialog : public wxDialog
{
public:
	FilterDialog (wxWindow *, std::vector<Filter const *> const &);

	boost::signals2::signal<void (std::vector<Filter const *>)> ActiveChanged;

private:
	void active_changed ();

	FilterEditor* _filters;
};

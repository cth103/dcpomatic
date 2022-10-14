/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


/** @file src/wx/film_editor.h
 *  @brief FilmEditor class.
 */


#include "lib/film.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class wxNotebook;
class Film;
class ContentPanel;
class DCPPanel;
class FilmViewer;


/** @class FilmEditor
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */
class FilmEditor : public wxPanel
{
public:
	FilmEditor(wxWindow *, FilmViewer& viewer);

	void set_film (std::shared_ptr<Film>);
	void first_shown ();

	boost::signals2::signal<void (void)> SelectionChanged;

	/* Stuff for panels */

	ContentPanel* content_panel () const {
		return _content_panel;
	}

	std::shared_ptr<Film> film () const {
		return _film;
	}

private:

	/* Handle changes to the model */
	void film_change (ChangeType, Film::Property);
	void film_content_change (ChangeType type, int);

	void set_general_sensitivity (bool);
	void active_jobs_changed (boost::optional<std::string>);

	ContentPanel* _content_panel;
	DCPPanel* _dcp_panel;

	/** The film we are editing */
	std::shared_ptr<Film> _film;
};

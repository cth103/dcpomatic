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

/** @file  src/film_viewer.h
 *  @brief A wx widget to view `thumbnails' of a Film.
 */

#include <wx/wx.h>
#include "lib/film.h"

class ThumbPanel;

/** @class FilmViewer
 *  @brief A wx widget to view `thumbnails' of a Film.
 */
class FilmViewer : public wxPanel
{
public:
	FilmViewer (Film *, wxWindow *);

	void set_film (Film *);
	void setup_visibility ();

private:
	void slider_changed (wxCommandEvent &);
	void update_thumbs ();
	void load_thumbnail (int);
	void film_changed (Film::Property);
	void reload_current_thumbnail ();

	Film* _film;
	wxBoxSizer* _sizer;
	ThumbPanel* _thumb_panel;
	wxSlider* _slider;
};

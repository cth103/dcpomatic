/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/dcpomatic_time.h"
#include "lib/film_property.h"
#include "lib/layout_markers.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <map>


class wxTipWindow;


class MarkersPanel : public wxPanel
{
public:
	MarkersPanel(wxWindow* parent, FilmViewer& viewer, bool allow_editing);

	void set_film(std::weak_ptr<Film> film);

private:
	void paint();
	void size();
	void mouse_moved(wxMouseEvent& ev);
	void mouse_left_down();
	void mouse_right_down(wxMouseEvent& ev);
	void move_marker_to_current_position();
	void remove_marker();
	void add_marker(wxCommandEvent& ev);
	void film_changed(ChangeType type, FilmProperty property);
	void layout();

	wxTipWindow* _tip = nullptr;

	std::weak_ptr<Film> _film;
	std::vector<MarkerLayoutComponent> _components;
	MarkerLayoutComponent const* _over = nullptr;
	FilmViewer& _viewer;
	MarkerLayoutComponent const* _menu_marker = nullptr;
	bool _allow_editing;
};


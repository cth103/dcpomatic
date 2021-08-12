/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_CONTENT_SUB_PANEL_H
#define DCPOMATIC_CONTENT_SUB_PANEL_H

#include "lib/film.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS

class ContentPanel;
class Content;
class DCPContent;
class wxGridBagSizer;

class ContentSubPanel : public wxScrolledWindow
{
public:
	ContentSubPanel (ContentPanel *, wxString);

	virtual void create () = 0;

	virtual void film_changed (Film::Property) {}
	/** Called when a given property of one of the selected Contents changes */
	virtual void film_content_changed (int) = 0;
	/** Called when the list of selected Contents changes */
	virtual void content_selection_changed () = 0;

	wxString name () const {
		return _name;
	}

protected:

	void setup_refer_button (wxCheckBox* button, wxStaticText* note, std::shared_ptr<DCPContent> dcp, bool can_reference, wxString cannot);
	void layout ();
	virtual void add_to_grid () = 0;

	ContentPanel* _parent;
	wxSizer* _sizer;
	wxGridBagSizer* _grid;
	wxString _name;
};

#endif

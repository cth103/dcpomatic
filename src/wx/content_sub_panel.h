/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <wx/wx.h>
#include "lib/film.h"

class ContentPanel;
class Content;
class DCPContent;

class ContentSubPanel : public wxScrolledWindow
{
public:
	ContentSubPanel (ContentPanel *, wxString);

	virtual void film_changed (Film::Property) {}
	/** Called when a given property of one of the selected Contents changes */
	virtual void film_content_changed (int) = 0;
	/** Called when the list of selected Contents changes */
	virtual void content_selection_changed () = 0;

	wxString name () const {
		return _name;
	}

protected:

	void setup_refer_button (wxCheckBox* button, wxStaticText* note, boost::shared_ptr<DCPContent> dcp, bool can_reference, std::string why_not) const;

	ContentPanel* _parent;
	wxSizer* _sizer;
	wxString _name;
};

#endif

/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include <wx/listctrl.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/filesystem.hpp>

class Content;
class TextContent;

class FontsDialog : public wxDialog
{
public:
	FontsDialog (wxWindow* parent, boost::shared_ptr<Content>, boost::shared_ptr<TextContent> caption);

private:
	void setup ();
	void setup_sensitivity ();
	void selection_changed ();
	void edit_clicked ();

	boost::weak_ptr<Content> _content;
	boost::weak_ptr<TextContent> _caption;
	wxListCtrl* _fonts;
	wxButton* _edit;
};

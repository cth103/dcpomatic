/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <boost/signals2.hpp>

class TemplatesDialog : public wxDialog
{
public:
	TemplatesDialog (wxWindow* parent);

	void refresh ();
	void layout ();

private:
	void selection_changed ();
	void rename_clicked ();
	void remove_clicked ();
	void resized (wxSizeEvent& ev);

	wxButton* _rename;
	wxButton* _remove;
	wxListCtrl* _list;
	wxBoxSizer* _sizer;

	boost::signals2::scoped_connection _config_connection;
};

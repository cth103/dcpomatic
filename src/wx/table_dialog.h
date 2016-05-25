/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TABLE_DIALOG_H
#define DCPOMATIC_TABLE_DIALOG_H

#include <wx/wx.h>

class TableDialog : public wxDialog
{
public:
	TableDialog (wxWindow* parent, wxString title, int columns, int growable, bool cancel);

protected:
	template<class T>
	T* add (T* w, int proportion = 1, int flag = wxEXPAND) {
		_table->Add (w, proportion, flag);
		return w;
	}

	void add (wxString text, bool label);
	void add_spacer ();

	void layout ();

	wxFlexGridSizer* _table;

private:
	wxSizer* _overall_sizer;
};

#endif

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

/** @file src/wx/wx_util.cc
 *  @brief Some utility functions.
 */

#include "wx_util.h"

using namespace std;

wxStaticText *
add_label_to_sizer (wxSizer* s, wxWindow* p, string t)
{
	wxStaticText* m = new wxStaticText (p, wxID_ANY, wxString (t.c_str (), wxConvUTF8));
	s->Add (m, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	return m;
}

void
error_dialog (wxWindow* parent, string m)
{
	wxMessageDialog* d = new wxMessageDialog (parent, std_to_wx (m), wxT ("DVD-o-matic"), wxOK);
	d->ShowModal ();
	d->Destroy ();
}

string
wx_to_std (wxString s)
{
	return string (s.mb_str ());
}

wxString
std_to_wx (string s)
{
	return wxString (s.c_str(), wxConvUTF8);
}

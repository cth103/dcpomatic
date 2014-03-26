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

#include "cinema_dialog.h"
#include "wx_util.h"

using std::string;

CinemaDialog::CinemaDialog (wxWindow* parent, string title, string name, string email)
	: TableDialog (parent, std_to_wx (title), 2, true)
{
	add (_("Name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (256, -1)));

	add (_("Email address for KDM delivery"), true);
	_email = add (new wxTextCtrl (this, wxID_ANY, std_to_wx (email), wxDefaultPosition, wxSize (256, -1)));

	layout ();
}

string
CinemaDialog::name () const
{
	return wx_to_std (_name->GetValue());
}

string
CinemaDialog::email () const
{
	return wx_to_std (_email->GetValue());
}

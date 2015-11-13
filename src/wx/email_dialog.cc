/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "email_dialog.h"
#include "wx_util.h"

using std::string;
using boost::shared_ptr;

EmailDialog::EmailDialog (wxWindow* parent)
	: TableDialog (parent, _("Email address"), 2, 1, true)
{
	add (_("Email address"), true);
	_email = add (new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (400, -1)));

	layout ();
}

void
EmailDialog::set (string address)
{
	_email->SetValue (std_to_wx (address));
}

string
EmailDialog::get () const
{
	return wx_to_std (_email->GetValue ());
}

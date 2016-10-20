/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "email_dialog.h"
#include "wx_util.h"

using std::string;
using boost::shared_ptr;
using boost::optional;

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

optional<string>
EmailDialog::get () const
{
	string s = wx_to_std (_email->GetValue ());
	if (s.empty ()) {
		/* Invalid email address */
		return optional<string> ();
	}

	return s;
}

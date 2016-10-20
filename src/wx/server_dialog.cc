/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "lib/encode_server.h"
#include "server_dialog.h"
#include "wx_util.h"

using std::string;
using boost::shared_ptr;
using boost::optional;

ServerDialog::ServerDialog (wxWindow* parent)
	: TableDialog (parent, _("Server"), 2, 1, true)
{
        wxClientDC dc (parent);
	/* XXX: bit of a mystery why we need such a long string here */
        wxSize size = dc.GetTextExtent (wxT ("255.255.255.255.255.255.255.255"));
        size.SetHeight (-1);

        wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
        wxArrayString list;

	add (_("Host name or IP address"), true);
	_host = add (new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size));

	layout ();
}

void
ServerDialog::set (string server)
{
	_host->SetValue (std_to_wx (server));
}

optional<string>
ServerDialog::get () const
{
	return wx_to_std (_host->GetValue ());
}

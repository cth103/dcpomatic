/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "servers_list_dialog.h"
#include "wx_util.h"
#include "lib/encode_server_finder.h"
#include "lib/encode_server_description.h"
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

using std::list;
using std::string;
using boost::lexical_cast;

ServersListDialog::ServersListDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Encoding Servers"))
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	SetSizer (s);

	_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (500, 200), wxLC_REPORT | wxLC_SINGLE_SEL);

	{
		wxListItem ip;
		ip.SetId (0);
		ip.SetText (_("Host"));
		ip.SetWidth (300);
		_list->InsertColumn (0, ip);
	}

	{
		wxListItem ip;
		ip.SetId (1);
		ip.SetText (_("Threads"));
		ip.SetWidth (150);
		_list->InsertColumn (1, ip);
	}

	s->Add (_list, 1, wxEXPAND | wxALL, 12);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		s->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (s);
	s->Layout ();
	s->SetSizeHints (this);

	_server_finder_connection = EncodeServerFinder::instance()->ServersListChanged.connect (
		boost::bind (&ServersListDialog::servers_list_changed, this)
		);
	servers_list_changed ();
}

void
ServersListDialog::servers_list_changed ()
{
	_list->DeleteAllItems ();

	int n = 0;

	BOOST_FOREACH (EncodeServerDescription i, EncodeServerFinder::instance()->servers()) {
		wxListItem list_item;
		list_item.SetId (n);
		_list->InsertItem (list_item);

		_list->SetItem (n, 0, std_to_wx (i.host_name ()));
		if (i.current_link_version()) {
			_list->SetItem (n, 1, std_to_wx (lexical_cast<string> (i.threads ())));
		} else {
			_list->SetItem (n, 1, _("Incorrect version"));
		}
		++n;
	}
}

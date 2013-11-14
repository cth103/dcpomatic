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

#include "lib/server.h"
#include "server_dialog.h"
#include "wx_util.h"

using std::string;
using boost::shared_ptr;

ServerDialog::ServerDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Server"))
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);

        wxClientDC dc (parent);
	/* XXX: bit of a mystery why we need such a long string here */
        wxSize size = dc.GetTextExtent (wxT ("255.255.255.255.255.255.255.255"));
        size.SetHeight (-1);

        wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
        wxArrayString list;

	add_label_to_sizer (table, this, _("Host name or IP address"), true);
	_host = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size);
	table->Add (_host, 1, wxEXPAND | wxALL);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

void
ServerDialog::set (string server)
{
	_host->SetValue (std_to_wx (server));
}

string
ServerDialog::get () const
{
	return wx_to_std (_host->GetValue ());
}


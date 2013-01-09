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
	: wxDialog (parent, wxID_ANY, std_to_wx (title))
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
	table->AddGrowableCol (1, 1);

	add_label_to_sizer (table, this, "Name");
	_name = new wxTextCtrl (this, wxID_ANY, std_to_wx (name), wxDefaultPosition, wxSize (256, -1));
	table->Add (_name, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Email address for KDM delivery");
	_email = new wxTextCtrl (this, wxID_ANY, std_to_wx (email), wxDefaultPosition, wxSize (256, -1));
	table->Add (_email, 1, wxEXPAND);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
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

/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "initial_setup_dialog.h"
#include "lib/config.h"
#include <boost/bind.hpp>

InitialSetupDialog::InitialSetupDialog ()
	: wxDialog (0, wxID_ANY, _("DCP-o-matic setup"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxStaticText* text1 = new wxStaticText (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, -1));
	sizer->Add (text1, 1, wxEXPAND | wxALL, 12);

	text1->SetLabelMarkup (
		_(
			"<span weight=\"bold\" size=\"larger\">Welcome to DCP-o-matic!</span>\n\n"
			"DCP-o-matic can work in two modes: '<i>simple</i>' or '<i>full</i>'.\n\n"
			"<i>Simple mode</i> is ideal for producing straightforward DCPs without too many confusing "
			"options.\n\n"
			"<i>Full mode</i> gives you the most control over the DCPs you make.\n\n"
			"Please choose which mode you would like to start DCP-o-matic in:\n\n"
			)
		);

	_simple = new wxRadioButton (this, wxID_ANY, _("Simple mode"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	sizer->Add (_simple, 0, wxLEFT, 24);
	_full = new wxRadioButton (this, wxID_ANY, _("Full mode"));
	sizer->Add (_full, 0, wxLEFT, 24);

	if (Config::instance()->interface_complexity() == Config::INTERFACE_SIMPLE) {
		_simple->SetValue (true);
	} else {
		_full->SetValue (true);
	}

	wxStaticText* text2 = new wxStaticText (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(400, -1));
	sizer->Add (text2, 0, wxEXPAND | wxALL, 12);

	text2->SetLabelMarkup (_("\nYou can change the mode at any time from the General page of Preferences."));

	_simple->Bind (wxEVT_RADIOBUTTON, boost::bind(&InitialSetupDialog::interface_complexity_changed, this));
	_full->Bind (wxEVT_RADIOBUTTON, boost::bind(&InitialSetupDialog::interface_complexity_changed, this));

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	sizer->Layout ();
	SetSizerAndFit (sizer);
}

void
InitialSetupDialog::interface_complexity_changed ()
{
	if (_simple->GetValue()) {
		Config::instance()->set_interface_complexity (Config::INTERFACE_SIMPLE);
	} else {
		Config::instance()->set_interface_complexity (Config::INTERFACE_FULL);
	}
}

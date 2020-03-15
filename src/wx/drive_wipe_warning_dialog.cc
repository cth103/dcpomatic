/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "drive_wipe_warning_dialog.h"
#include "static_text.h"
#include "wx_util.h"

DriveWipeWarningDialog::DriveWipeWarningDialog (wxWindow* parent, wxString drive)
	: wxDialog (parent, wxID_ANY, _("Important notice"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxStaticText* text = new StaticText (this, wxEmptyString, wxDefaultPosition, wxSize(400, 300));
	sizer->Add (text, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	_yes = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_yes, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	text->SetLabelMarkup (
		wxString::Format(
			_("If you continue with this operation <span weight=\"bold\" size=\"larger\">ALL DATA</span> "
			  "on the drive %s will be <span weight=\"bold\" size=\"larger\">PERMANENTLY DESTROYED</span>.\n\n"
			  "If you are sure you want to continue please type \"yes\" into the box below, then click OK."), drive
			)
		);
}

bool
DriveWipeWarningDialog::confirmed () const
{
	return _yes->GetValue() == "yes";
}

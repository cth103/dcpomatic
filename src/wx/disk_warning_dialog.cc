/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "disk_warning_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"


DiskWarningDialog::DiskWarningDialog ()
	: wxDialog(nullptr, wxID_ANY, _("Important notice"))
{
	auto sizer = new wxBoxSizer (wxVERTICAL);
	auto text = new StaticText (this, wxEmptyString, wxDefaultPosition, wxSize(400, 300));
	sizer->Add (text, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	_yes = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_yes, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	/// TRANSLATORS: the user will be asked to type this phrase into a text entry to confirm that they have read
	/// the warning about using the disk writer.
	auto const confirmation = _("I am sure");

	text->SetLabelMarkup(wxString::Format(
		_("The <b>%s</b> is\n\n<span weight=\"bold\" size=\"20480\" foreground=\"red\">BETA-GRADE TEST SOFTWARE</span>\n\n"
	          "and may\n\n<span weight=\"bold\" size=\"20480\" foreground=\"red\">DESTROY DATA!</span>\n\n"
		  "If you are sure you want to continue please type\n\n<tt>%s</tt>\n\ninto the box below, then click OK."),
		variant::wx::dcpomatic_disk_writer(), confirmation));
}

bool
DiskWarningDialog::confirmed () const
{
	return _yes->GetValue() == _("I am sure");
}

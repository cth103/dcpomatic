/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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


#include "static_text.h"
#include "update_dialog.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/version.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/hyperlink.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using boost::optional;


UpdateDialog::UpdateDialog(wxWindow* parent, optional<string> stable, optional<string> test)
	: wxDialog(parent, wxID_ANY, _("Update"))
{
	auto overall_sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* message;

	if ((stable || test) && !(stable && test)) {
		message = new StaticText(this, variant::wx::insert_dcpomatic(_("A new version of %s is available.")));
	} else {
		message = new StaticText(this, variant::wx::insert_dcpomatic(_("New versions of %s are available.")));
	}

	overall_sizer->Add(message, 0, wxTOP | wxLEFT | wxRIGHT, DCPOMATIC_DIALOG_BORDER);

	auto table = new wxFlexGridSizer(3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	if (stable) {
		add_label_to_sizer(table, this, _("Stable version"), true, 0, wxALIGN_CENTER_VERTICAL);
		add_label_to_sizer(table, this, std_to_wx(stable.get()), true, 0, wxALIGN_CENTER_VERTICAL);
		auto h = new wxHyperlinkCtrl(this, wxID_ANY, char_to_wx("dcpomatic.com/download"), char_to_wx("https://dcpomatic.com/download"));
		table->Add(h, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_DIALOG_BORDER);
	}

	if (test) {
		add_label_to_sizer(table, this, _("Test version"), true, 0, wxALIGN_CENTER_VERTICAL);
		add_label_to_sizer(table, this, std_to_wx(test.get()), true, 0, wxALIGN_CENTER_VERTICAL);
		auto h = new wxHyperlinkCtrl(this, wxID_ANY, char_to_wx("dcpomatic.com/test-download"), char_to_wx("https://dcpomatic.com/test-download"));
		table->Add(h, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_DIALOG_BORDER);
	}

	add_label_to_sizer(table, this, _("Current version"), true, 0, wxALIGN_CENTER_VERTICAL);
	add_label_to_sizer(table, this, char_to_wx(dcpomatic_version), true, 0, wxALIGN_CENTER_VERTICAL);

	overall_sizer->Add(table, 0, wxTOP | wxLEFT | wxRIGHT, DCPOMATIC_DIALOG_BORDER);

	if (auto buttons = CreateButtonSizer(wxOK)) {
		overall_sizer->Add(buttons, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	}

	SetSizerAndFit(overall_sizer);
}

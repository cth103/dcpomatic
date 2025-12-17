/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "editable_list.h"
#include "extra_kdm_email_dialog.h"
#include "wx_util.h"
#include <wx/gbsizer.h>
#include <vector>


using std::string;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


ExtraKDMEmailDialog::ExtraKDMEmailDialog (wxWindow* parent, vector<string> emails)
	: wxDialog (parent, wxID_ANY, _("Extra addresses for KDM delivery"))
	, _emails(emails)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	auto sizer = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	int r = 0;

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Address"), 500, true));
	_email_list = new EditableList<string>(
		this,
		columns,
		bind(&ExtraKDMEmailDialog::get, this),
		bind(&ExtraKDMEmailDialog::set, this, _1),
		EditableList<string>::add_with_dialog<EmailDialog>,
		EditableList<string>::edit_with_dialog<EmailDialog>,
		[](string s, int) { return s; },
		EditableListTitle::INVISIBLE,
		EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);

	sizer->Add (_email_list, wxGBPosition(r, 0), wxGBSpan(1, 2), wxEXPAND);
	++r;

	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}


vector<string>
ExtraKDMEmailDialog::get () const
{
	return _emails;
}


void
ExtraKDMEmailDialog::set(vector<string> emails)
{
	_emails = emails;
}


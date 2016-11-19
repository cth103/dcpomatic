/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "confirm_kdm_email_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/cinema_kdms.h"
#include <boost/foreach.hpp>

using std::list;
using std::string;

ConfirmKDMEmailDialog::ConfirmKDMEmailDialog (wxWindow* parent, list<string> emails)
	: wxDialog (parent, wxID_ANY, _("Confirm KDM email"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxString message = _("Are you sure you want to send emails to the following addresses?\n\n");
	BOOST_FOREACH (string i, emails) {
		message += "\t" + std_to_wx (i) + "\n";
	}

	sizer->Add (new wxStaticText (this, wxID_ANY, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxCheckBox* shut_up = new wxCheckBox (this, wxID_ANY, _("Don't ask this again"));
	sizer->Add (shut_up, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	shut_up->Bind (wxEVT_CHECKBOX, bind (&ConfirmKDMEmailDialog::shut_up, this, _1));

	wxStdDialogButtonSizer* buttons = CreateStdDialogButtonSizer (0);
	sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	buttons->SetAffirmativeButton (new wxButton (this, wxID_OK, _("Send emails")));
	buttons->SetNegativeButton (new wxButton (this, wxID_CANCEL, _("Don't send emails")));
	buttons->Realize ();

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}

void
ConfirmKDMEmailDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_confirm_kdm_email (!ev.IsChecked ());
}

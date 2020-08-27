/*
    Copyright (C) 2016-2018 Carl Hetherington <cth@carlh.net>

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
#include "static_text.h"
#include "check_box.h"
#include "lib/config.h"
#include <boost/foreach.hpp>

using std::list;
using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

ConfirmKDMEmailDialog::ConfirmKDMEmailDialog (wxWindow* parent, list<string> emails)
	: QuestionDialog (parent, _("Confirm KDM email"), _("Send emails"), _("Don't send emails"))
{
	wxString message = _("Are you sure you want to send emails to the following addresses?\n\n");
	BOOST_FOREACH (string i, emails) {
		message += "\t" + std_to_wx (i) + "\n";
	}

	_sizer->Add (new StaticText (this, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxCheckBox* shut_up = new CheckBox (this, _("Don't ask this again"));
	_sizer->Add (shut_up, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	shut_up->Bind (wxEVT_CHECKBOX, bind (&ConfirmKDMEmailDialog::shut_up, this, _1));

	layout ();
}

void
ConfirmKDMEmailDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_confirm_kdm_email (!ev.IsChecked ());
}

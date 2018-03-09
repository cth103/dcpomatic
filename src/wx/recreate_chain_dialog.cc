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

#include "recreate_chain_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/cinema_kdms.h"
#include <boost/foreach.hpp>

using std::list;
using std::string;

RecreateChainDialog::RecreateChainDialog (wxWindow* parent)
	: QuestionDialog (parent, _("Certificate chain"), _("Recreate signing certificates"), _("Do nothing"))
{
	wxString const message = _("The certificate chain that DCP-o-matic uses for signing DCPs and KDMs contains a small error\n"
				   "which will prevent DCPs from being validated correctly on some systems.  Do you want to re-create\n"
				   "the certificate chain for signing DCPs and KDMs?");

	_sizer->Add (new wxStaticText (this, wxID_ANY, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxCheckBox* shut_up = new wxCheckBox (this, wxID_ANY, _("Don't ask this again"));
	_sizer->Add (shut_up, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	shut_up->Bind (wxEVT_CHECKBOX, bind (&RecreateChainDialog::shut_up, this, _1));

	layout ();
}

void
RecreateChainDialog::shut_up (wxCommandEvent& ev)
{
	std::cout << "set nagged " << ev.IsChecked() << "\n";
	Config::instance()->set_nagged (Config::NAG_BAD_SIGNER_CHAIN, ev.IsChecked());
}

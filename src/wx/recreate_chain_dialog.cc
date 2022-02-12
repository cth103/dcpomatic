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
#include "static_text.h"
#include "check_box.h"
#include "lib/config.h"
#include "lib/cinema_kdms.h"
#include <boost/foreach.hpp>

using std::list;
using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

RecreateChainDialog::RecreateChainDialog (wxWindow* parent, Config::BadSignerChainReason reason)
	: QuestionDialog (parent, _("Certificate chain"), _("Recreate signing certificates"), _("Do nothing"))
	, _reason (reason)
{
	wxString message;
	if (_reason & Config::BAD_SIGNER_CHAIN_VALIDITY_TOO_LONG) {
		message = _("The certificate chain that DCP-o-matic uses for signing DCPs and KDMs has a validity period\n"
			    "that is too long.  This will cause problems playing back DCPs on some systems.\n"
			    "Do you want to re-create the certificate chain for signing DCPs and KDMs?");
	} else {
		message = _("The certificate chain that DCP-o-matic uses for signing DCPs and KDMs contains a small error\n"
			    "which will prevent DCPs from being validated correctly on some systems.  Do you want to re-create\n"
			    "the certificate chain for signing DCPs and KDMs?");
	}

	_sizer->Add (new StaticText (this, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxCheckBox* shut_up = new CheckBox (this, _("Don't ask this again"));
	_sizer->Add (shut_up, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	shut_up->Bind (wxEVT_CHECKBOX, bind (&RecreateChainDialog::shut_up, this, _1));

	layout ();
}

void
RecreateChainDialog::shut_up (wxCommandEvent& ev)
{
	if (_reason & Config::BAD_SIGNER_CHAIN_VALIDITY_TOO_LONG) {
		Config::instance()->set_nagged (Config::NAG_BAD_SIGNER_CHAIN_VALIDITY_TOO_LONG, ev.IsChecked());
	} else {
		Config::instance()->set_nagged (Config::NAG_BAD_SIGNER_CHAIN_UTF8_STRINGS, ev.IsChecked());
	}
}


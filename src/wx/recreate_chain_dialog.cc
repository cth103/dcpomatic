/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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

using std::list;
using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

RecreateChainDialog::RecreateChainDialog (wxWindow* parent, wxString title, wxString message, wxString cancel, optional<Config::Nag> nag)
	: QuestionDialog (parent, _("Certificate chain"), title, cancel)
	, _nag (nag)
{
	_sizer->Add (new StaticText (this, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	if (nag) {
		wxCheckBox* shut_up = new CheckBox (this, _("Don't ask this again"));
		_sizer->Add (shut_up, 0, wxALL, DCPOMATIC_DIALOG_BORDER);
		shut_up->Bind (wxEVT_CHECKBOX, bind (&RecreateChainDialog::shut_up, this, _1));
	}

	layout ();
}

void
RecreateChainDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_nagged(*_nag, ev.IsChecked());
}

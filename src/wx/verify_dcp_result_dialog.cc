/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "verify_dcp_result_dialog.h"
#include "verify_dcp_result_panel.h"
#include "wx_util.h"


using std::shared_ptr;


VerifyDCPResultDialog::VerifyDCPResultDialog(wxWindow* parent, shared_ptr<VerifyDCPJob> job)
	: wxDialog (parent, wxID_ANY, _("DCP verification"), wxDefaultPosition, {600, 400})
{
	auto sizer = new wxBoxSizer (wxVERTICAL);

	auto panel = new VerifyDCPResultPanel(this);
	panel->fill(job);
	sizer->Add(panel, 1, wxEXPAND);

	auto buttons = CreateStdDialogButtonSizer(0);
	sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	buttons->SetAffirmativeButton (new wxButton (this, wxID_OK));
	buttons->Realize ();

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

}

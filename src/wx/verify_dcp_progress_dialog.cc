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


#include "verify_dcp_progress_dialog.h"
#include "verify_dcp_progress_panel.h"
#include "wx_util.h"
#include "lib/cross.h"
#include "lib/job_manager.h"
#include "lib/verify_dcp_job.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
LIBDCP_ENABLE_WARNINGS
#include <string>


using std::string;
using std::shared_ptr;
using boost::optional;


VerifyDCPProgressDialog::VerifyDCPProgressDialog (wxWindow* parent, wxString title)
	: wxDialog (parent, wxID_ANY, title)
	, _panel(new VerifyDCPProgressPanel(this))
	, _cancel (false)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);

	overall_sizer->Add(_panel, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	auto cancel = new wxButton(this, wxID_ANY, _("Cancel"));
	auto buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->AddStretchSpacer ();
	buttons->Add (cancel, 0);
	overall_sizer->Add (buttons, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	SetSizerAndFit (overall_sizer);

	cancel->Bind (wxEVT_BUTTON, boost::bind(&VerifyDCPProgressDialog::cancel, this));
}


void
VerifyDCPProgressDialog::cancel ()
{
	_cancel = true;
}


bool
VerifyDCPProgressDialog::run(shared_ptr<VerifyDCPJob> job)
{
	Show ();

	JobManager* jm = JobManager::instance ();
	jm->add (job);

	while (jm->work_to_do()) {
		wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
		dcpomatic_sleep_seconds (1);

		_panel->update(job);

		if (_cancel) {
			break;
		}
	}

	return !_cancel;
}

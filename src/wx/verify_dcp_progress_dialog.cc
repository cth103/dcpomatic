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


#include "check_box.h"
#include "dcpomatic_button.h"
#include "verify_dcp_progress_dialog.h"
#include "verify_dcp_progress_panel.h"
#include "verify_dcp_result_panel.h"
#include "wx_util.h"
#include "lib/cross.h"
#include "lib/job_manager.h"
#include "lib/verify_dcp_job.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
LIBDCP_ENABLE_WARNINGS
#include <string>


using std::shared_ptr;
using std::string;
using boost::optional;


VerifyDCPProgressDialog::VerifyDCPProgressDialog (wxWindow* parent, wxString title, shared_ptr<VerifyDCPJob> job)
	: wxDialog (parent, wxID_ANY, title)
	, _progress_panel(new VerifyDCPProgressPanel(this))
	, _result_panel(new VerifyDCPResultPanel(this))
	, _cancel_pending(false)
	, _job(job)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);

	auto options_sizer = new wxBoxSizer(wxVERTICAL);
	_check_picture_details = new CheckBox(this, _("Verify picture asset details"));
	_check_picture_details->set(true);
	_check_picture_details->SetToolTip(
		_("Tick to check details of the picture asset, such as frame sizes and JPEG2000 bitstream validity.  "
		  "These checks are quite time-consuming.")
	);
	options_sizer->Add(_check_picture_details, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
	overall_sizer->Add(options_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);

	auto buttons = new wxBoxSizer(wxHORIZONTAL);
	_cancel = new Button(this, _("Cancel"));
	buttons->Add(_cancel, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
	_verify = new Button(this, _("Verify"));
	buttons->Add(_verify, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
	overall_sizer->Add(buttons, 0, wxALL | wxALIGN_CENTER, DCPOMATIC_SIZER_GAP);

	overall_sizer->Add(_progress_panel, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);
	overall_sizer->Add(_result_panel, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	SetSizerAndFit (overall_sizer);

	_verify->bind(&VerifyDCPProgressDialog::verify_clicked, this);
	_cancel->bind(&VerifyDCPProgressDialog::cancel_clicked, this);

	_cancel->Enable(false);
}


void
VerifyDCPProgressDialog::cancel_clicked()
{
	_cancel_pending = true;
}


void
VerifyDCPProgressDialog::verify_clicked()
{
	_cancel->Enable(true);
	_verify->Enable(false);

	auto jm = JobManager::instance();
	jm->add(_job);

	while (jm->work_to_do() && !_cancel_pending) {
		wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
		dcpomatic_sleep_milliseconds(250);
		_progress_panel->update(_job);
	}

	if (_cancel_pending) {
		jm->cancel_all_jobs();
		EndModal(0);
	} else {
		_progress_panel->clear();
		_result_panel->add({ _job });
		_cancel->Enable(false);
		_verify->Enable(false);
		_check_picture_details->Enable(false);
	}
}

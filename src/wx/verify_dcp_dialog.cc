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
#include "verify_dcp_dialog.h"
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
using std::vector;
using boost::optional;


VerifyDCPDialog::VerifyDCPDialog(
	wxWindow* parent,
	wxString title,
	vector<boost::filesystem::path> dcp_directories,
	vector<boost::filesystem::path> kdms
	)
	: wxDialog (parent, wxID_ANY, title)
	, _progress_panel(new VerifyDCPProgressPanel(this))
	, _result_panel(new VerifyDCPResultPanel(this))
	, _cancel_pending(false)
	, _dcp_directories(std::move(dcp_directories))
{
	setup();

	if (auto key = Config::instance()->decryption_chain()->key()) {
		for (auto const& kdm: kdms) {
			_kdms.push_back(dcp::DecryptedKDM{dcp::EncryptedKDM(dcp::file_to_string(kdm)), *key});
		}
	}
}


VerifyDCPDialog::VerifyDCPDialog(
	wxWindow* parent,
	wxString title,
	vector<boost::filesystem::path> dcp_directories,
	vector<dcp::EncryptedKDM> const& kdms
	)
	: wxDialog (parent, wxID_ANY, title)
	, _progress_panel(new VerifyDCPProgressPanel(this))
	, _result_panel(new VerifyDCPResultPanel(this))
	, _cancel_pending(false)
	, _dcp_directories(std::move(dcp_directories))
{
	setup();

	if (auto key = Config::instance()->decryption_chain()->key()) {
		for (auto const& kdm: kdms) {
			_kdms.push_back(dcp::DecryptedKDM{kdm, *key});
		}
	}
}


void
VerifyDCPDialog::setup()
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

	SetSizerAndFit(overall_sizer);

	_verify->bind(&VerifyDCPDialog::verify_clicked, this);
	_cancel->bind(&VerifyDCPDialog::cancel_clicked, this);

	_cancel->Enable(false);
}


void
VerifyDCPDialog::cancel_clicked()
{
	_cancel_pending = true;
}


void
VerifyDCPDialog::verify_clicked()
{
	_cancel->Enable(true);
	_verify->Enable(false);

	dcp::VerificationOptions options;
	options.check_picture_details = _check_picture_details->get();
	auto job = std::make_shared<VerifyDCPJob>(_dcp_directories, _kdms, options);

	auto jm = JobManager::instance();
	jm->add(job);

	while (jm->work_to_do() && !_cancel_pending) {
		wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
		dcpomatic_sleep_milliseconds(250);
		_progress_panel->update(job);
	}

	if (_cancel_pending) {
		jm->cancel_all_jobs();
		EndModal(0);
	} else {
		_progress_panel->clear();
		_result_panel->add({ job });
		_cancel->Enable(false);
		_verify->Enable(false);
		_check_picture_details->Enable(false);
	}
}

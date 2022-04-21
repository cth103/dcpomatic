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
#include "wx_util.h"
#include "lib/cross.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include <wx/evtloop.h>
#include <string>


using std::string;
using std::shared_ptr;
using boost::optional;


static int const max_file_name_length = 80;


VerifyDCPProgressDialog::VerifyDCPProgressDialog (wxWindow* parent, wxString title)
	: wxDialog (parent, wxID_ANY, title)
	, _cancel (false)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);

	_job_name = new wxStaticText (this, wxID_ANY, wxT(""));
	overall_sizer->Add (_job_name, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);

	_file_name = new wxStaticText (this, wxID_ANY, wxT(""));
	wxFont file_name_font (*wxNORMAL_FONT);
	file_name_font.SetFamily (wxFONTFAMILY_MODERN);
	file_name_font.SetPointSize (file_name_font.GetPointSize() - 2);
	_file_name->SetFont (file_name_font);

	int w;
	int h;
	_file_name->GetTextExtent (std_to_wx(string(max_file_name_length, 'X')), &w, &h);
	_file_name->SetMinSize (wxSize(w, -1));

	overall_sizer->Add (_file_name, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, DCPOMATIC_SIZER_GAP);

	_progress = new wxGauge (this, wxID_ANY, 100);
	overall_sizer->Add (_progress, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_GAP);

	wxButton* cancel = new wxButton (this, wxID_ANY, _("Cancel"));
	wxSizer* buttons = new wxBoxSizer (wxHORIZONTAL);
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
VerifyDCPProgressDialog::run (shared_ptr<Job> job)
{
	Show ();

	JobManager* jm = JobManager::instance ();
	jm->add (job);

	while (jm->work_to_do()) {
		wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
		dcpomatic_sleep_seconds (1);
		optional<float> const progress = job->progress ();
		if (progress) {
			_progress->SetValue (*progress * 100);
		} else {
			_progress->Pulse ();
		}
		string const sub = job->sub_name ();
		size_t colon = sub.find (":");
		if (colon != string::npos) {
			_job_name->SetLabel (std_to_wx(sub.substr(0, colon)));
			string file_name;
			if ((sub.length() - colon - 1) > max_file_name_length) {
				file_name = "..." + sub.substr(sub.length() - max_file_name_length + 3);
			} else {
				file_name = sub.substr(colon + 1);
			}
			_file_name->SetLabel (std_to_wx(file_name));
		} else {
			_job_name->SetLabel (std_to_wx(sub));
		}

		if (_cancel) {
			break;
		}
	}

	return !_cancel;
}

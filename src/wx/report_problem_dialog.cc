/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "report_problem_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/job_manager.h"
#include "lib/send_problem_report_job.h"
#include <wx/sizer.h>

using std::string;
using boost::shared_ptr;

/** @param film Film that we are working on, or 0 */
ReportProblemDialog::ReportProblemDialog (wxWindow* parent, shared_ptr<Film> film)
	: wxDialog (parent, wxID_ANY, _("Report A Problem"))
	, _film (film)
{
	_overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_overall_sizer);

	_table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_table->AddGrowableCol (1, 1);

	_overall_sizer->Add (_table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		_overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	wxString t = _("My problem is");
	int flags = wxALIGN_TOP | wxLEFT | wxRIGHT;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
	t += wxT (":");
#endif
	wxStaticText* m = new wxStaticText (this, wxID_ANY, t);
	_table->Add (m, 1, flags, 6);

	_summary = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, wxSize (320, 240), wxTE_MULTILINE);
	_table->Add (_summary, 1, wxEXPAND | wxALIGN_TOP);

	_send_logs = new wxCheckBox (this, wxID_ANY, _("Send logs"));
	_send_logs->SetValue (true);
	_table->Add (_send_logs, 1, wxEXPAND);
	_table->AddSpacer (0);

	add_label_to_sizer (_table, this, _("Your email address"), true);
	_email = new wxTextCtrl (this, wxID_ANY, wxT (""));
	_email->SetValue (std_to_wx (Config::instance()->kdm_from ()));
	_table->Add (_email, 1, wxEXPAND);

	/* We can't use Wrap() here as it doesn't work with markup:
	 * http://trac.wxwidgets.org/ticket/13389
	 */

	wxString in = _("<i>It is important that you enter a valid email address here, otherwise I can't ask you for more details on your problem.</i>");
	wxString out;
	int const width = 45;
	int current = 0;
	for (size_t i = 0; i < in.Length(); ++i) {
		if (in[i] == ' ' && current >= width) {
			out += '\n';
			current = 0;
		} else {
			out += in[i];
			++current;
		}
	}

	wxStaticText* n = new wxStaticText (this, wxID_ANY, wxT (""));
	n->SetLabelMarkup (out);
	_table->AddSpacer (0);
	_table->Add (n, 1, wxEXPAND);

	_overall_sizer->Layout ();
	_overall_sizer->SetSizeHints (this);
}

void
ReportProblemDialog::report ()
{
	if (_email->GetValue().IsEmpty ()) {
		error_dialog (this, _("Please enter an email address so that we can contact you with any queries about the problem."));
		return;
	}

	if (_email->GetValue() == "carl@dcpomatic.com" || _email->GetValue() == "cth@carlh.net") {
		error_dialog (this, wxString::Format (_("Enter your email address for the contact, not %s"), _email->GetValue().data()));
		return;
	}

	JobManager::instance()->add (shared_ptr<Job> (new SendProblemReportJob (_film, wx_to_std (_email->GetValue ()), wx_to_std (_summary->GetValue ()))));
}

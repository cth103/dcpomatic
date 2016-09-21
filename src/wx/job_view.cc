/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "job_view.h"
#include "wx_util.h"
#include "lib/job.h"
#include "lib/compose.hpp"
#include <wx/wx.h>

using std::string;
using std::min;
using boost::shared_ptr;

JobView::JobView (shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table)
	: _job (job)
	, _parent (parent)
{
	int n = 0;

	_gauge_message = new wxBoxSizer (wxVERTICAL);
	_gauge = new wxGauge (container, wxID_ANY, 100);
	/* This seems to be required to allow the gauge to shrink under OS X */
	_gauge->SetMinSize (wxSize (0, -1));
	_gauge_message->Add (_gauge, 0, wxEXPAND | wxLEFT | wxRIGHT);
	_message = new wxStaticText (container, wxID_ANY, wxT (" \n "));
	_gauge_message->Add (_message, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	table->Insert (n, _gauge_message, 1, wxEXPAND | wxLEFT | wxRIGHT);
	++n;

	_cancel = new wxButton (container, wxID_ANY, _("Cancel"));
	_cancel->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobView::cancel_clicked, this);
	table->Insert (n, _cancel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
	++n;

	_pause = new wxButton (container, wxID_ANY, _("Pause"));
	_pause->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobView::pause_clicked, this);
	table->Insert (n, _pause, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
	++n;

	_details = new wxButton (container, wxID_ANY, _("Details..."));
	_details->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobView::details_clicked, this);
	_details->Enable (false);
	table->Insert (n, _details, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
	++n;

	_progress_connection = job->Progress.connect (boost::bind (&JobView::progress, this));
	_finished_connection = job->Finished.connect (boost::bind (&JobView::finished, this));

	progress ();

	table->Layout ();
}

void
JobView::maybe_pulse ()
{
	if (_job->running() && !_job->progress ()) {
		_gauge->Pulse ();
	}
}

void
JobView::progress ()
{
	string whole = "<b>" + _job->name () + "</b>\n";
	if (!_job->sub_name().empty ()) {
		whole += _job->sub_name() + " ";
	}
	whole += _job->status ();
	if (whole != _last_message) {
		_message->SetLabelMarkup (std_to_wx (whole));
		_gauge_message->Layout ();
		_last_message = whole;
	}
	if (_job->progress ()) {
		_gauge->SetValue (min (100.0f, _job->progress().get() * 100));
	}
}

void
JobView::finished ()
{
	progress ();

	if (!_job->finished_cancelled ()) {
		_gauge->SetValue (100);
	}

	_cancel->Enable (false);
	_pause->Enable (false);
	if (!_job->error_details().empty ()) {
		_details->Enable (true);
	}
}

void
JobView::details_clicked (wxCommandEvent &)
{
	string s = _job->error_summary();
	s[0] = toupper (s[0]);
	error_dialog (_parent, std_to_wx (String::compose ("%1.\n\n%2", s, _job->error_details())));
}

void
JobView::cancel_clicked (wxCommandEvent &)
{
	if (confirm_dialog (_parent, _("Are you sure you want to cancel this job?"))) {
		_job->cancel ();
	}
}

void
JobView::pause_clicked (wxCommandEvent &)
{
	if (_job->paused()) {
		_job->resume ();
		_pause->SetLabel (_("Pause"));
	} else {
		_job->pause ();
		_pause->SetLabel (_("Resume"));
	}
}

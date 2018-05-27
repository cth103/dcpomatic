/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
#include "lib/config.h"
#include <wx/wx.h>

using std::string;
using std::min;
using boost::shared_ptr;
using boost::bind;

JobView::JobView (shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table)
	: _job (job)
	, _table (table)
	, _parent (parent)
	, _container (container)
	, _gauge (0)
{

}

void
JobView::setup ()
{
	int n = insert_position ();

	_gauge_message = new wxBoxSizer (wxVERTICAL);
	_gauge = new wxGauge (_container, wxID_ANY, 100);
	/* This seems to be required to allow the gauge to shrink under OS X */
	_gauge->SetMinSize (wxSize (0, -1));
	_gauge_message->Add (_gauge, 0, wxEXPAND | wxLEFT | wxRIGHT);
	_message = new wxStaticText (_container, wxID_ANY, wxT (" \n "), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
	_gauge_message->Add (_message, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	_table->Insert (n, _gauge_message, 1, wxEXPAND | wxLEFT | wxRIGHT);
	++n;

	_buttons = new wxBoxSizer (wxHORIZONTAL);

	_cancel = new wxButton (_container, wxID_ANY, _("Cancel"));
	_cancel->Bind (wxEVT_BUTTON, &JobView::cancel_clicked, this);
	_buttons->Add (_cancel, 1, wxALIGN_CENTER_VERTICAL);

	_details = new wxButton (_container, wxID_ANY, _("Details..."));
	_details->Bind (wxEVT_BUTTON, &JobView::details_clicked, this);
	_details->Enable (false);
	_buttons->Add (_details, 1, wxALIGN_CENTER_VERTICAL);

	finish_setup (_container, _buttons);

	_controls = new wxBoxSizer (wxVERTICAL);
	_controls->Add (_buttons);
	_notify = new wxCheckBox (_container, wxID_ANY, _("Notify when complete"));
	_notify->Bind (wxEVT_CHECKBOX, bind (&JobView::notify_clicked, this));
	_notify->SetValue (Config::instance()->default_notify());
	_controls->Add (_notify);

	_table->Insert (n, _controls, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);

	_progress_connection = _job->Progress.connect (boost::bind (&JobView::progress, this));
	_finished_connection = _job->Finished.connect (boost::bind (&JobView::finished, this));

	progress ();

	_table->Layout ();
}

void
JobView::maybe_pulse ()
{
	if (_gauge && _job->running() && !_job->progress()) {
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
		/* This hack fixes the size of _message on OS X */
		_message->InvalidateBestSize ();
		_message->SetSize (_message->GetBestSize ());
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
	_notify->Enable (false);
	if (!_job->error_details().empty ()) {
		_details->Enable (true);
	}

	if (_notify->GetValue()) {
		if (Config::instance()->notification(Config::MESSAGE_BOX)) {
			wxMessageBox (std_to_wx(_job->name() + ": " + _job->status()), _("DCP-o-matic"), wxICON_INFORMATION);
		}
		if (Config::instance()->notification(Config::EMAIL)) {

		}
	}
}

void
JobView::details_clicked (wxCommandEvent &)
{
	string s = _job->error_summary();
	s[0] = toupper (s[0]);
	error_dialog (_parent, std_to_wx(s), std_to_wx(_job->error_details()));
}

void
JobView::cancel_clicked (wxCommandEvent &)
{
	if (confirm_dialog (_parent, _("Are you sure you want to cancel this job?"))) {
		_job->cancel ();
	}
}

void
JobView::insert (int pos)
{
	_table->Insert (pos, _gauge_message, 1, wxEXPAND | wxLEFT | wxRIGHT);
	_table->Insert (pos + 1, _controls, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
	_table->Layout ();
}

void
JobView::detach ()
{
	_table->Detach (_gauge_message);
	_table->Detach (_controls);
}

void
JobView::notify_clicked ()
{
	Config::instance()->set_default_notify (_notify->GetValue ());
}

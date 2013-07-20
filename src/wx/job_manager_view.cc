/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file src/job_manager_view.cc
 *  @brief Class generating a GTK widget to show the progress of jobs.
 */

#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/util.h"
#include "lib/exceptions.h"
#include "job_manager_view.h"
#include "wx_util.h"

using std::string;
using std::list;
using std::map;
using boost::shared_ptr;
using boost::weak_ptr;

/** Must be called in the GUI thread */
JobManagerView::JobManagerView (wxWindow* parent, Buttons buttons)
	: wxScrolledWindow (parent)
	, _buttons (buttons)
{
	_panel = new wxPanel (this);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	sizer->Add (_panel, 1, wxEXPAND);
	SetSizer (sizer);

	int N = 5;
	if (buttons & PAUSE) {
		++N;
	}
	
	_table = new wxFlexGridSizer (N, 6, 6);
	_table->AddGrowableCol (1, 1);
	_panel->SetSizer (_table);

	SetScrollRate (0, 32);

	JobManager::instance()->JobAdded.connect (bind (&JobManagerView::job_added, this, _1));
}

void
JobManagerView::job_added (weak_ptr<Job> j)
{
	shared_ptr<Job> job = j.lock ();
	if (!job) {
		return;
	}

	wxStaticText* m = new wxStaticText (_panel, wxID_ANY, std_to_wx (job->name ()));
	_table->Insert (0, m, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	
	JobRecord r;
	int n = 1;
	r.scroll_nudged = false;
	r.gauge = new wxGauge (_panel, wxID_ANY, 100);
	_table->Insert (n, r.gauge, 1, wxEXPAND | wxLEFT | wxRIGHT);
	++n;
	
	r.message = new wxStaticText (_panel, wxID_ANY, std_to_wx (""));
	_table->Insert (n, r.message, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	++n;
	
	r.cancel = new wxButton (_panel, wxID_ANY, _("Cancel"));
	r.cancel->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::cancel_clicked), 0, this);
	_table->Insert (n, r.cancel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	++n;
	
	if (_buttons & PAUSE) {
		r.pause = new wxButton (_panel, wxID_ANY, _("Pause"));
		r.pause->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::pause_clicked), 0, this);
		_table->Insert (n, r.pause, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	}
	
	r.details = new wxButton (_panel, wxID_ANY, _("Details..."));
	r.details->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::details_clicked), 0, this);
	r.details->Enable (false);
	_table->Insert (n, r.details, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
	++n;
	
	_job_records[job] = r;

	job->Progress.connect (bind (&JobManagerView::progress, this, j));
	job->Progress.connect (bind (&JobManagerView::finished, this, j));
	
	_table->Layout ();
	FitInside ();
}

void
JobManagerView::progress (weak_ptr<Job> j)
{
	shared_ptr<Job> job = j.lock ();
	if (!job) {
		return;
	}

	float const p = job->overall_progress ();
	if (p >= 0) {
		checked_set (_job_records[job].message, job->status ());
		_job_records[job].gauge->SetValue (p * 100);
	} else {
		checked_set (_job_records[job].message, wx_to_std (_("Running")));
		_job_records[job].gauge->Pulse ();
	}

	if (!_job_records[job].scroll_nudged && (job->running () || job->finished())) {
		int x, y;
		_job_records[job].gauge->GetPosition (&x, &y);
		int px, py;
		GetScrollPixelsPerUnit (&px, &py);
		int vx, vy;
		GetViewStart (&vx, &vy);
		int sx, sy;
		GetClientSize (&sx, &sy);
		
		if (y > (vy * py + sy / 2)) {
			Scroll (-1, y / py);
			_job_records[job].scroll_nudged = true;
		}
	}

	_table->Layout ();
	FitInside ();
}

void
JobManagerView::finished (weak_ptr<Job> j)
{
	shared_ptr<Job> job = j.lock ();
	if (!job) {
		return;
	}
	
	checked_set (_job_records[job].message, job->status ());
	if (!job->finished_cancelled ()) {
		_job_records[job].gauge->SetValue (100);
	}
	_job_records[job].cancel->Enable (false);
	if (!job->error_details().empty ()) {
		_job_records[job].details->Enable (true);
	}

	_table->Layout ();
	FitInside ();
}

void
JobManagerView::details_clicked (wxCommandEvent& ev)
{
	wxObject* o = ev.GetEventObject ();

	for (map<shared_ptr<Job>, JobRecord>::iterator i = _job_records.begin(); i != _job_records.end(); ++i) {
		if (i->second.details == o) {
			string s = i->first->error_summary();
			s[0] = toupper (s[0]);
			error_dialog (this, std_to_wx (String::compose ("%1.\n\n%2", s, i->first->error_details())));
		}
	}
}

void
JobManagerView::cancel_clicked (wxCommandEvent& ev)
{
	wxObject* o = ev.GetEventObject ();

	for (map<shared_ptr<Job>, JobRecord>::iterator i = _job_records.begin(); i != _job_records.end(); ++i) {
		if (i->second.cancel == o) {
			i->first->cancel ();
		}
	}
}

void
JobManagerView::pause_clicked (wxCommandEvent& ev)
{
	wxObject* o = ev.GetEventObject ();
	for (map<boost::shared_ptr<Job>, JobRecord>::iterator i = _job_records.begin(); i != _job_records.end(); ++i) {
		if (i->second.pause == o) {
			if (i->first->paused()) {
				i->first->resume ();
				i->second.pause->SetLabel (_("Pause"));
			} else {
				i->first->pause ();
				i->second.pause->SetLabel (_("Resume"));
			}
		}
	}
}
	

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

	Connect (wxID_ANY, wxEVT_TIMER, wxTimerEventHandler (JobManagerView::periodic), 0, this);
	_timer.reset (new wxTimer (this));
	_timer->Start (1000);

	update ();
}

void
JobManagerView::periodic (wxTimerEvent &)
{
	update ();
}

/** Update the view by examining the state of each job.
 *  Must be called in the GUI thread.
 */
void
JobManagerView::update ()
{
	list<shared_ptr<Job> > jobs = JobManager::instance()->get ();

	int index = 0;

	for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
		
		if (_job_records.find (*i) == _job_records.end ()) {
			wxStaticText* m = new wxStaticText (_panel, wxID_ANY, std_to_wx ((*i)->name ()));
			_table->Insert (index, m, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
			
			JobRecord r;
			int n = 1;
			r.finalised = false;
			r.scroll_nudged = false;
			r.gauge = new wxGauge (_panel, wxID_ANY, 100);
			_table->Insert (index + n, r.gauge, 1, wxEXPAND | wxLEFT | wxRIGHT);
			++n;
			
			r.message = new wxStaticText (_panel, wxID_ANY, std_to_wx (""));
			_table->Insert (index + n, r.message, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
			++n;

			r.cancel = new wxButton (_panel, wxID_ANY, _("Cancel"));
			r.cancel->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::cancel_clicked), 0, this);
			_table->Insert (index + n, r.cancel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
			++n;

			if (_buttons & PAUSE) {
				r.pause = new wxButton (_panel, wxID_ANY, _("Pause"));
				r.pause->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::pause_clicked), 0, this);
				_table->Insert (index + n, r.pause, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
				++n;
			}
			
			r.details = new wxButton (_panel, wxID_ANY, _("Details..."));
			r.details->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (JobManagerView::details_clicked), 0, this);
			r.details->Enable (false);
			_table->Insert (index + n, r.details, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
			++n;
			
			_job_records[*i] = r;

		}

		string const st = (*i)->status ();

		if (!(*i)->finished ()) {
			float const p = (*i)->overall_progress ();
			if (p >= 0) {
				checked_set (_job_records[*i].message, st);
				_job_records[*i].gauge->SetValue (p * 100);
			} else {
				checked_set (_job_records[*i].message, wx_to_std (_("Running")));
				_job_records[*i].gauge->Pulse ();
			}

		}

		if (!_job_records[*i].scroll_nudged && ((*i)->running () || (*i)->finished())) {
			int x, y;
			_job_records[*i].gauge->GetPosition (&x, &y);
			int px, py;
			GetScrollPixelsPerUnit (&px, &py);
			int vx, vy;
			GetViewStart (&vx, &vy);
			int sx, sy;
			GetClientSize (&sx, &sy);

			if (y > (vy * py + sy / 2)) {
				Scroll (-1, y / py);
				_job_records[*i].scroll_nudged = true;
			}
		}
			
		if ((*i)->finished() && !_job_records[*i].finalised) {
			checked_set (_job_records[*i].message, st);
			if (!(*i)->finished_cancelled()) {
				_job_records[*i].gauge->SetValue (100);
			}
			_job_records[*i].finalised = true;
			_job_records[*i].cancel->Enable (false);
			if (!(*i)->error_details().empty ()) {
				_job_records[*i].details->Enable (true);
			}
		}

		index += 5;
		if (_buttons & PAUSE) {
			++index;
		}
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
	

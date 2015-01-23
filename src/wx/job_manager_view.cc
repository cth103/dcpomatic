/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;

class JobRecord
{
public:
	JobRecord (shared_ptr<Job> job, wxScrolledWindow* window, wxPanel* panel, wxFlexGridSizer* table)
		: _job (job)
		, _window (window)
		, _panel (panel)
		, _table (table)
	{
		int n = 0;
		
		_name = new wxStaticText (panel, wxID_ANY, "");
		string const jn = "<b>" + _job->name () + "</b>";
		_name->SetLabelMarkup (std_to_wx (jn));
		table->Insert (n, _name, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	
		_gauge = new wxGauge (panel, wxID_ANY, 100);
		/* This seems to be required to allow the gauge to shrink under OS X */
		_gauge->SetMinSize (wxSize (0, -1));
		table->Insert (n, _gauge, 1, wxEXPAND | wxLEFT | wxRIGHT);
		++n;
		
		_message = new wxStaticText (panel, wxID_ANY, std_to_wx (""));
		table->Insert (n, _message, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	
		_cancel = new wxButton (panel, wxID_ANY, _("Cancel"));
		_cancel->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobRecord::cancel_clicked, this);
		table->Insert (n, _cancel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	
		_pause = new wxButton (_panel, wxID_ANY, _("Pause"));
		_pause->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobRecord::pause_clicked, this);
		table->Insert (n, _pause, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	
		_details = new wxButton (_panel, wxID_ANY, _("Details..."));
		_details->Bind (wxEVT_COMMAND_BUTTON_CLICKED, &JobRecord::details_clicked, this);
		_details->Enable (false);
		table->Insert (n, _details, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
		++n;
	
		job->Progress.connect (boost::bind (&JobRecord::progress, this));
		job->Finished.connect (boost::bind (&JobRecord::finished, this));
	
		table->Layout ();
		panel->FitInside ();
	}

	void maybe_pulse ()
	{
		if (_job->running() && !_job->progress ()) {
			_gauge->Pulse ();
		}
	}

private:

	void update_job_name ()
	{
		string n = "<b>" + _job->name () + "</b>";
		if (!_job->sub_name().empty ()) {
			n += "\n" + _job->sub_name ();
		}
		
		if (n != _last_name) {
			_name->SetLabelMarkup (std_to_wx (n));
			_last_name = n;
		}
	}

	void progress ()
	{
		checked_set (_message, _job->status ());
		update_job_name ();
		if (_job->progress ()) {
			_gauge->SetValue (min (100.0f, _job->progress().get() * 100));
 		}
		_table->Layout ();
		_window->FitInside ();
	}

	void finished ()
	{
		checked_set (_message, _job->status ());
		update_job_name ();
		
		if (!_job->finished_cancelled ()) {
			_gauge->SetValue (100);
		}
		
		_cancel->Enable (false);
		_pause->Enable (false);
		if (!_job->error_details().empty ()) {
			_details->Enable (true);
		}
		
		_table->Layout ();
		_window->FitInside ();
	}

	void details_clicked (wxCommandEvent &)
	{
		string s = _job->error_summary();
		s[0] = toupper (s[0]);
		error_dialog (_window, std_to_wx (String::compose ("%1.\n\n%2", s, _job->error_details())));
	}
	
	void cancel_clicked (wxCommandEvent &)
	{
		_job->cancel ();
	}

	void pause_clicked (wxCommandEvent &)
	{
		if (_job->paused()) {
			_job->resume ();
			_pause->SetLabel (_("Pause"));
		} else {
			_job->pause ();
			_pause->SetLabel (_("Resume"));
		}
	}
	
	boost::shared_ptr<Job> _job;
	wxScrolledWindow* _window;
	wxPanel* _panel;
	wxFlexGridSizer* _table;
	wxStaticText* _name;
	wxGauge* _gauge;
	wxStaticText* _message;
	wxButton* _cancel;
	wxButton* _pause;
	wxButton* _details;
	std::string _last_name;
};

/** Must be called in the GUI thread */
JobManagerView::JobManagerView (wxWindow* parent)
	: wxScrolledWindow (parent)
{
	_panel = new wxPanel (this);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	sizer->Add (_panel, 1, wxEXPAND);
	SetSizer (sizer);

	_table = new wxFlexGridSizer (6, 6, 6);
	_table->AddGrowableCol (1, 1);
	_panel->SetSizer (_table);

	SetScrollRate (0, 32);
	EnableScrolling (false, true);

	Bind (wxEVT_TIMER, boost::bind (&JobManagerView::periodic, this));
	_timer.reset (new wxTimer (this));
	_timer->Start (1000);

	Bind (wxEVT_SIZE, boost::bind (&JobManagerView::sized, this, _1));
	JobManager::instance()->JobAdded.connect (bind (&JobManagerView::job_added, this, _1));
}

void
JobManagerView::sized (wxSizeEvent& ev)
{
	_table->FitInside (_panel);
	_table->Layout ();
	ev.Skip ();
}

void
JobManagerView::job_added (weak_ptr<Job> j)
{
	shared_ptr<Job> job = j.lock ();
	if (job) {
		_job_records.push_back (shared_ptr<JobRecord> (new JobRecord (job, this, _panel, _table)));
	}
}

void
JobManagerView::periodic ()
{
	for (list<shared_ptr<JobRecord> >::iterator i = _job_records.begin(); i != _job_records.end(); ++i) {
		(*i)->maybe_pulse ();
	}
}

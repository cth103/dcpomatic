/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "job_view_dialog.h"
#include "normal_job_view.h"
#include "lib/job.h"

using std::shared_ptr;

JobViewDialog::JobViewDialog (wxWindow* parent, wxString title, shared_ptr<Job> job)
	: TableDialog (parent, title, 4, 0, false)
	, _job (job)
{
	_view = new NormalJobView (job, this, this, _table);
	_view->setup ();
	layout ();
	SetMinSize (wxSize (960, -1));

	Bind (wxEVT_TIMER, boost::bind (&JobViewDialog::periodic, this));
	_timer.reset (new wxTimer (this));
	_timer->Start (1000);

	/* Start off with OK disabled and it will be enabled when the job is finished */
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (false);
	}
}

JobViewDialog::~JobViewDialog ()
{
	delete _view;
}

void
JobViewDialog::periodic ()
{
	_view->maybe_pulse ();

	shared_ptr<Job> job = _job.lock ();
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (job && ok) {
		ok->Enable (job->finished ());
	}
}

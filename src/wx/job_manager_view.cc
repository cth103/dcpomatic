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

#include "job_manager_view.h"
#include "job_view.h"
#include "wx_util.h"
#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/util.h"
#include "lib/exceptions.h"
#include "lib/compose.hpp"
#include <iostream>

using std::string;
using std::list;
using std::map;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;

/** Must be called in the GUI thread */
JobManagerView::JobManagerView (wxWindow* parent)
	: wxScrolledWindow (parent)
{
	_panel = new wxPanel (this);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	sizer->Add (_panel, 1, wxEXPAND);
	SetSizer (sizer);

	_table = new wxFlexGridSizer (4, 4, 6);
	_table->AddGrowableCol (0, 1);
	_panel->SetSizer (_table);

	SetScrollRate (0, 32);
	EnableScrolling (false, true);

	Bind (wxEVT_TIMER, boost::bind (&JobManagerView::periodic, this));
	_timer.reset (new wxTimer (this));
	_timer->Start (1000);

	JobManager::instance()->JobAdded.connect (bind (&JobManagerView::job_added, this, _1));
}

void
JobManagerView::job_added (weak_ptr<Job> j)
{
	shared_ptr<Job> job = j.lock ();
	if (job) {
		_job_records.push_back (shared_ptr<JobView> (new JobView (job, this, _panel, _table)));
	}
}

void
JobManagerView::periodic ()
{
	for (list<shared_ptr<JobView> >::iterator i = _job_records.begin(); i != _job_records.end(); ++i) {
		(*i)->maybe_pulse ();
	}
}

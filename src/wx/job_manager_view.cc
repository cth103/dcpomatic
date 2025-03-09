/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/job_manager_view.cc
 *  @brief Class generating a GTK widget to show the progress of jobs.
 */


#include "batch_job_view.h"
#include "job_manager_view.h"
#include "normal_job_view.h"
#include "wx_util.h"
#include "lib/compose.hpp"
#include "lib/exceptions.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include <iostream>


using std::cout;
using std::list;
using std::make_shared;
using std::map;
using std::min;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @param parent Parent window.
 *  @param batch true to use BatchJobView, false to use NormalJobView.
 *
 *  Must be called in the GUI thread.
 */
JobManagerView::JobManagerView(wxWindow* parent, bool batch)
	: wxScrolledWindow(parent)
	, _timer(this)
	, _batch(batch)
{
	_panel = new wxPanel(this);
	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(_panel, 1, wxEXPAND);
	SetSizer(sizer);

	_table = new wxFlexGridSizer(2, 6, 6);
	_table->AddGrowableCol(0, 1);
	_panel->SetSizer(_table);

	SetScrollRate(0, 32);
	EnableScrolling(false, true);

	Bind(wxEVT_TIMER, boost::bind(&JobManagerView::periodic, this));
	_timer.Start(1000);

	JobManager::instance()->JobAdded.connect(bind(&JobManagerView::job_added, this, _1));
	JobManager::instance()->JobsReordered.connect(bind(&JobManagerView::replace, this));
}


void
JobManagerView::job_added(weak_ptr<Job> j)
{
	if (auto job = j.lock()) {
		shared_ptr<JobView> v;
		if (_batch) {
			v = make_shared<BatchJobView>(job, this, _panel, _table);
		} else {
			v = make_shared<NormalJobView>(job, this, _panel, _table);
		}
		v->setup();
		_job_records.push_back(v);
	}

	FitInside();
	job_list_changed();
}


void
JobManagerView::periodic()
{
	for (auto i: _job_records) {
		i->maybe_pulse();
	}
}


void
JobManagerView::replace()
{
	/* Make a new version of _job_records which reflects the order in JobManager's job list */

	list<shared_ptr<JobView>> new_job_records;

	for (auto i: JobManager::instance()->get()) {
		/* Find this job's JobView */
		for (auto j: _job_records) {
			if (j->job() == i) {
				new_job_records.push_back(j);
				break;
			}
		}
	}

	for (auto i: _job_records) {
		i->detach();
	}

	_job_records = new_job_records;

	for (auto i: _job_records) {
		i->insert(i->insert_position());
	}

	job_list_changed();
}


void
JobManagerView::job_list_changed()
{
	for (auto i: _job_records) {
		i->job_list_changed();
	}
}

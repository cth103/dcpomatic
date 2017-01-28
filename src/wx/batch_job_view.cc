/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

#include "batch_job_view.h"
#include "lib/job_manager.h"
#include <wx/sizer.h>
#include <wx/button.h>

using std::list;
using boost::shared_ptr;

BatchJobView::BatchJobView (shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table)
	: JobView (job, parent, container, table)
{

}

int
BatchJobView::insert_position () const
{
	return _table->GetEffectiveRowsCount() * _table->GetEffectiveColsCount();
}

void
BatchJobView::finish_setup (wxWindow* parent, wxSizer* sizer)
{
	_higher_priority = new wxButton (parent, wxID_ANY, _("Higher prioirity"));
	_higher_priority->Bind (wxEVT_BUTTON, boost::bind (&BatchJobView::higher_priority_clicked, this));
	sizer->Add (_higher_priority, 1, wxALIGN_CENTER_VERTICAL);
	_lower_priority = new wxButton (parent, wxID_ANY, _("Lower prioirity"));
	_lower_priority->Bind (wxEVT_BUTTON, boost::bind (&BatchJobView::lower_priority_clicked, this));
	sizer->Add (_lower_priority, 1, wxALIGN_CENTER_VERTICAL);
}
void
BatchJobView::higher_priority_clicked ()
{
	JobManager::instance()->increase_priority (_job);
}

void
BatchJobView::lower_priority_clicked ()
{
	JobManager::instance()->decrease_priority (_job);
}

void
BatchJobView::job_list_changed ()
{
	bool high = false;
	bool low = false;
	list<shared_ptr<Job> > jobs = JobManager::instance()->get();
	if (!jobs.empty ()) {
		if (_job != jobs.front()) {
			high = true;
		}
		if (_job != jobs.back()) {
			low = true;
		}
	}

	_higher_priority->Enable (high);
	_lower_priority->Enable (low);
}

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


#include "normal_job_view.h"
#include "dcpomatic_button.h"
#include "lib/job.h"
#include <wx/wx.h>


using std::shared_ptr;


NormalJobView::NormalJobView (shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table)
	: JobView (job, parent, container, table)
{

}


void
NormalJobView::finish_setup (wxWindow* parent, wxSizer* sizer)
{
	_pause = new Button (parent, _("Pause"));
	_pause->Bind (wxEVT_BUTTON, boost::bind (&NormalJobView::pause_clicked, this));
	sizer->Add (_pause, 1, wxALIGN_CENTER_VERTICAL);
}

int

NormalJobView::insert_position () const
{
	return 0;
}


void
NormalJobView::pause_clicked ()
{
	if (_job->paused_by_user()) {
		_job->resume ();
		_pause->SetLabel (_("Pause"));
	} else {
		_job->pause_by_user ();
		_pause->SetLabel (_("Resume"));
	}
}


void
NormalJobView::finished ()
{
	JobView::finished ();
	_pause->Enable (false);
}

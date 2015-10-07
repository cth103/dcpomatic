/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "job_view_dialog.h"
#include "job_view.h"

using boost::shared_ptr;

JobViewDialog::JobViewDialog (wxWindow* parent, wxString title, shared_ptr<Job> job)
	: TableDialog (parent, title, 4, 0, false)
{
	_view = new JobView (job, this, this, _table);
	layout ();
	SetMinSize (wxSize (960, -1));

	Bind (wxEVT_TIMER, boost::bind (&JobViewDialog::periodic, this));
	_timer.reset (new wxTimer (this));
	_timer->Start (1000);
}

JobViewDialog::~JobViewDialog ()
{
	delete _view;
}

void
JobViewDialog::periodic ()
{
	_view->maybe_pulse ();
}

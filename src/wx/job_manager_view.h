/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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


/** @file src/job_manager_view.h
 *  @brief Class which is a wxPanel for showing the progress of jobs.
 */


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <list>
#include <memory>


class Job;
class JobView;


/** @class JobManagerView
 *  @brief Class which is a wxPanel for showing the progress of jobs.
 */
class JobManagerView : public wxScrolledWindow
{
public:
	JobManagerView (wxWindow *, bool batch);

private:
	void job_added (std::weak_ptr<Job>);
	void periodic ();
	void replace ();
	void job_list_changed ();

	wxPanel* _panel;
	wxFlexGridSizer* _table;
	std::shared_ptr<wxTimer> _timer;
	bool _batch;

	std::list<std::shared_ptr<JobView>> _job_records;
};

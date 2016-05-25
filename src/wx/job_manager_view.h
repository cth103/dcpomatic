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

#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <list>

class Job;
class JobView;

/** @class JobManagerView
 *  @brief Class which is a wxPanel for showing the progress of jobs.
 */
class JobManagerView : public wxScrolledWindow
{
public:
	JobManagerView (wxWindow *);

private:
	void job_added (boost::weak_ptr<Job>);
	void periodic ();

	wxPanel* _panel;
	wxFlexGridSizer* _table;
	boost::shared_ptr<wxTimer> _timer;

	std::list<boost::shared_ptr<JobView> > _job_records;
};

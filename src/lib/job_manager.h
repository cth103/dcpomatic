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

/** @file  src/job_manager.h
 *  @brief A simple scheduler for jobs.
 */

#include <list>
#include <boost/thread/mutex.hpp>
#include <boost/signals2.hpp>

class Job;

/** @class JobManager
 *  @brief A simple scheduler for jobs.
 */
class JobManager
{
public:

	boost::shared_ptr<Job> add (boost::shared_ptr<Job>);
	void add_after (boost::shared_ptr<Job> after, boost::shared_ptr<Job> j);
	std::list<boost::shared_ptr<Job> > get () const;
	bool work_to_do () const;
	bool errors () const;

	boost::signals2::signal<void (bool)> ActiveJobsChanged;

	static JobManager* instance ();

private:
	/* This function is part of the test suite */
	friend void ::wait_for_jobs ();
	
	JobManager ();
	void scheduler ();
	
	mutable boost::mutex _mutex;
	std::list<boost::shared_ptr<Job> > _jobs;

	bool _last_active_jobs;

	static JobManager* _instance;
};

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

class Job;

/** @class JobManager
 *  @brief A simple scheduler for jobs.
 *
 *  JobManager simply keeps a list of pending jobs, and assumes that all the jobs
 *  are sufficiently CPU intensive that there is no point running them in parallel;
 *  so jobs are just run one after the other.
 */
class JobManager
{
public:

	void add (boost::shared_ptr<Job>);
	void add_after (boost::shared_ptr<Job> after, boost::shared_ptr<Job> j);
	std::list<boost::shared_ptr<Job> > get () const;
	bool work_to_do () const;

	static JobManager* instance ();

private:
	JobManager ();
	void scheduler ();
	
	mutable boost::mutex _mutex;
	std::list<boost::shared_ptr<Job> > _jobs;

	static JobManager* _instance;
};

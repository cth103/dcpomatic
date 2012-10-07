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

/** @file  src/job_manager.cc
 *  @brief A simple scheduler for jobs.
 */

#include <iostream>
#include <boost/thread.hpp>
#include "job_manager.h"
#include "job.h"
#include "cross.h"

using namespace std;
using namespace boost;

JobManager* JobManager::_instance = 0;

JobManager::JobManager ()
{
	boost::thread (boost::bind (&JobManager::scheduler, this));
}

void
JobManager::add (shared_ptr<Job> j)
{
	boost::mutex::scoped_lock lm (_mutex);
	_jobs.push_back (j);
}

void
JobManager::add_after (shared_ptr<Job> after, shared_ptr<Job> j)
{
	boost::mutex::scoped_lock lm (_mutex);
	list<shared_ptr<Job> >::iterator i = find (_jobs.begin(), _jobs.end(), after);
	assert (i != _jobs.end ());
	++i;
	_jobs.insert (i, j);
}

list<shared_ptr<Job> >
JobManager::get () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _jobs;
}

bool
JobManager::work_to_do () const
{
	boost::mutex::scoped_lock lm (_mutex);
	list<shared_ptr<Job> >::const_iterator i = _jobs.begin();
	while (i != _jobs.end() && (*i)->finished()) {
		++i;
	}

	return i != _jobs.end ();
}

bool
JobManager::errors () const
{
	boost::mutex::scoped_lock lm (_mutex);
	for (list<shared_ptr<Job> >::const_iterator i = _jobs.begin(); i != _jobs.end(); ++i) {
		if ((*i)->finished_in_error ()) {
			return true;
		}
	}

	return false;
}	


void
JobManager::scheduler ()
{
	while (1) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			int running = 0;
			shared_ptr<Job> first_new;
			for (list<shared_ptr<Job> >::iterator i = _jobs.begin(); i != _jobs.end(); ++i) {
				if ((*i)->running ()) {
					++running;
				} else if (!(*i)->finished () && first_new == 0) {
					first_new = *i;
				}

				if (running == 0 && first_new) {
					first_new->start ();
					break;
				}
			}
		}

		dvdomatic_sleep (1);
	}
}

JobManager *
JobManager::instance ()
{
	if (_instance == 0) {
		_instance = new JobManager ();
	}

	return _instance;
}

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

/** @file  src/job_manager.cc
 *  @brief A simple scheduler for jobs.
 */

#include "job_manager.h"
#include "job.h"
#include "cross.h"
#include "analyse_audio_job.h"
#include "film.h"
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::function;
using boost::dynamic_pointer_cast;
using boost::optional;

JobManager* JobManager::_instance = 0;

JobManager::JobManager ()
	: _terminate (false)
	, _scheduler (0)
{

}

void
JobManager::start ()
{
	_scheduler = new boost::thread (boost::bind (&JobManager::scheduler, this));
}

JobManager::~JobManager ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
	}

	if (_scheduler) {
		DCPOMATIC_ASSERT (_scheduler->joinable ());
		_scheduler->join ();
	}

	delete _scheduler;
}

shared_ptr<Job>
JobManager::add (shared_ptr<Job> j)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_jobs.push_back (j);
	}

	emit (boost::bind (boost::ref (JobAdded), weak_ptr<Job> (j)));

	return j;
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
	while (true) {

		optional<string> active_job;

		{
			boost::mutex::scoped_lock lm (_mutex);
			if (_terminate) {
				return;
			}

			BOOST_FOREACH (shared_ptr<Job> i, _jobs) {

				if (!i->finished ()) {
					active_job = i->json_name ();
				}

				if (i->running ()) {
					/* Something is already happening */
					break;
				}

				if (i->is_new()) {
					i->start ();
					/* Only start one job at once */
					break;
				}
			}
		}

		if (active_job != _last_active_job) {
			emit (boost::bind (boost::ref (ActiveJobsChanged), _last_active_job, active_job));
			_last_active_job = active_job;
		}

		dcpomatic_sleep (1);
	}
}

JobManager *
JobManager::instance ()
{
	if (_instance == 0) {
		_instance = new JobManager ();
		_instance->start ();
	}

	return _instance;
}

void
JobManager::drop ()
{
	delete _instance;
	_instance = 0;
}

void
JobManager::analyse_audio (
	shared_ptr<const Film> film,
	shared_ptr<const Playlist> playlist,
	boost::signals2::connection& connection,
	function<void()> ready
	)
{
	{
		boost::mutex::scoped_lock lm (_mutex);

		BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
			shared_ptr<AnalyseAudioJob> a = dynamic_pointer_cast<AnalyseAudioJob> (i);
			if (a && a->playlist () == playlist) {
				i->when_finished (connection, ready);
				return;
			}
		}
	}

	shared_ptr<AnalyseAudioJob> job;

	{
		boost::mutex::scoped_lock lm (_mutex);

		job.reset (new AnalyseAudioJob (film, playlist));
		connection = job->Finished.connect (ready);
		_jobs.push_back (job);
	}

	emit (boost::bind (boost::ref (JobAdded), weak_ptr<Job> (job)));
}

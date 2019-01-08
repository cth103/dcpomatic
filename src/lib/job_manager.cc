/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
using boost::bind;

JobManager* JobManager::_instance = 0;

JobManager::JobManager ()
	: _terminate (false)
	, _paused (false)
	, _scheduler (0)
{

}

void
JobManager::start ()
{
	_scheduler = new boost::thread (boost::bind (&JobManager::scheduler, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_scheduler->native_handle(), "job-scheduler");
#endif
}

JobManager::~JobManager ()
{
	BOOST_FOREACH (boost::signals2::connection& i, _connections) {
		i.disconnect ();
	}

	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
		_empty_condition.notify_all ();
	}

	if (_scheduler) {
		/* Ideally this would be a DCPOMATIC_ASSERT(_scheduler->joinable()) but we
		   can't throw exceptions from a destructor.
		*/
		if (_scheduler->joinable ()) {
			_scheduler->join ();
		}
	}

	delete _scheduler;
}

shared_ptr<Job>
JobManager::add (shared_ptr<Job> j)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_jobs.push_back (j);
		_empty_condition.notify_all ();
	}

	emit (boost::bind (boost::ref (JobAdded), weak_ptr<Job> (j)));

	return j;
}

shared_ptr<Job>
JobManager::add_after (shared_ptr<Job> after, shared_ptr<Job> j)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		list<shared_ptr<Job> >::iterator i = find (_jobs.begin(), _jobs.end(), after);
		DCPOMATIC_ASSERT (i != _jobs.end());
		_jobs.insert (i, j);
		_empty_condition.notify_all ();
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
	BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
		if (i->finished_in_error ()) {
			return true;
		}
	}

	return false;
}

void
JobManager::scheduler ()
{
	while (true) {

		boost::mutex::scoped_lock lm (_mutex);

		while (true) {
			bool have_new = false;
			bool have_running = false;
			BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
				if (i->running()) {
					have_running = true;
				}
				if (i->is_new()) {
					have_new = true;
				}
			}

			if ((!have_running && have_new) || _terminate) {
				break;
			}

			_empty_condition.wait (lm);
		}

		if (_terminate) {
			break;
		}

		BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
			if (i->is_new()) {
				_connections.push_back (i->FinishedImmediate.connect(bind(&JobManager::job_finished, this)));
				i->start ();
				emit (boost::bind (boost::ref (ActiveJobsChanged), _last_active_job, i->json_name()));
				_last_active_job = i->json_name ();
				/* Only start one job at once */
				break;
			}
		}
	}
}

void
JobManager::job_finished ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		emit (boost::bind (boost::ref (ActiveJobsChanged), _last_active_job, optional<string>()));
		_last_active_job = optional<string>();
	}

	_empty_condition.notify_all ();
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
	bool from_zero,
	boost::signals2::connection& connection,
	function<void()> ready
	)
{
	{
		boost::mutex::scoped_lock lm (_mutex);

		BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
			shared_ptr<AnalyseAudioJob> a = dynamic_pointer_cast<AnalyseAudioJob> (i);
			if (a && a->path() == film->audio_analysis_path(playlist)) {
				i->when_finished (connection, ready);
				return;
			}
		}
	}

	shared_ptr<AnalyseAudioJob> job;

	{
		boost::mutex::scoped_lock lm (_mutex);

		job.reset (new AnalyseAudioJob (film, playlist, from_zero));
		connection = job->Finished.connect (ready);
		_jobs.push_back (job);
		_empty_condition.notify_all ();
	}

	emit (boost::bind (boost::ref (JobAdded), weak_ptr<Job> (job)));
}

void
JobManager::increase_priority (shared_ptr<Job> job)
{
	bool changed = false;

	{
		boost::mutex::scoped_lock lm (_mutex);
		list<shared_ptr<Job> >::iterator last = _jobs.end ();
		for (list<shared_ptr<Job> >::iterator i = _jobs.begin(); i != _jobs.end(); ++i) {
			if (*i == job && last != _jobs.end()) {
				swap (*i, *last);
				changed = true;
				break;
			}
			last = i;
		}
	}

	if (changed) {
		priority_changed ();
	}
}

void
JobManager::priority_changed ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);

		bool first = true;
		BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
			if (first) {
				if (i->is_new ()) {
					i->start ();
				} else if (i->paused_by_priority ()) {
					i->resume ();
				}
				first = false;
			} else {
				if (i->running ()) {
					i->pause_by_priority ();
				}
			}
		}
	}

	emit (boost::bind (boost::ref (JobsReordered)));
}

void
JobManager::decrease_priority (shared_ptr<Job> job)
{
	bool changed = false;

	{
		boost::mutex::scoped_lock lm (_mutex);
		for (list<shared_ptr<Job> >::iterator i = _jobs.begin(); i != _jobs.end(); ++i) {
			list<shared_ptr<Job> >::iterator next = i;
			++next;
			if (*i == job && next != _jobs.end()) {
				swap (*i, *next);
				changed = true;
				break;
			}
		}
	}

	if (changed) {
		priority_changed ();
	}
}

void
JobManager::pause ()
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_paused) {
		return;
	}

	BOOST_FOREACH (shared_ptr<Job> i, _jobs) {
		if (i->pause_by_user()) {
			_paused_job = i;
		}
	}

	_paused = true;
}

void
JobManager::resume ()
{
	boost::mutex::scoped_lock lm (_mutex);
	if (!_paused) {
		return;
	}

	if (_paused_job) {
		_paused_job->resume ();
	}

	_paused_job.reset ();
	_paused = false;
}

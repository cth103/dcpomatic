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


/** @file  src/job_manager.cc
 *  @brief A simple scheduler for jobs.
 */


#include "analyse_audio_job.h"
#include "analyse_subtitles_job.h"
#include "cross.h"
#include "film.h"
#include "job.h"
#include "job_manager.h"
#include "util.h"
#include <boost/thread.hpp>


using std::dynamic_pointer_cast;
using std::function;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::bind;
using boost::optional;


JobManager* JobManager::_instance = nullptr;


JobManager::JobManager ()
{

}


void
JobManager::start ()
{
	_scheduler = boost::thread (boost::bind(&JobManager::scheduler, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_scheduler.native_handle(), "job-scheduler");
#endif
}


JobManager::~JobManager ()
{
	boost::this_thread::disable_interruption dis;

	for (auto& i: _connections) {
		i.disconnect ();
	}

	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
		_schedule_condition.notify_all();
	}

	try {
		_scheduler.join();
	} catch (...) {}
}


shared_ptr<Job>
JobManager::add (shared_ptr<Job> j)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_jobs.push_back (j);
		_schedule_condition.notify_all();
	}

	emit (boost::bind(boost::ref(JobAdded), weak_ptr<Job>(j)));

	return j;
}


shared_ptr<Job>
JobManager::add_after (shared_ptr<Job> after, shared_ptr<Job> j)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		auto i = find (_jobs.begin(), _jobs.end(), after);
		DCPOMATIC_ASSERT (i != _jobs.end());
		_jobs.insert (i, j);
		_schedule_condition.notify_all();
	}

	emit (boost::bind(boost::ref(JobAdded), weak_ptr<Job>(j)));

	return j;
}


list<shared_ptr<Job>>
JobManager::get () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _jobs;
}


bool
JobManager::work_to_do () const
{
	boost::mutex::scoped_lock lm (_mutex);
	auto i = _jobs.begin();
	while (i != _jobs.end() && (*i)->finished()) {
		++i;
	}

	return i != _jobs.end ();
}


bool
JobManager::errors () const
{
	boost::mutex::scoped_lock lm (_mutex);
	for (auto i: _jobs) {
		if (i->finished_in_error()) {
			return true;
		}
	}

	return false;
}


void
JobManager::scheduler ()
{
	start_of_thread ("JobManager");

	while (true) {

		boost::mutex::scoped_lock lm (_mutex);

		if (_terminate) {
			break;
		}

		bool have_running = false;
		for (auto i: _jobs) {
			if ((have_running || _paused) && i->running()) {
				/* We already have a running job, or are totally paused, so this job should not be running */
				i->pause_by_priority();
			} else if (!have_running && !_paused && (i->is_new() || i->paused_by_priority())) {
				/* We don't have a running job, and we should have one, so start/resume this */
				if (i->is_new()) {
					_connections.push_back (i->FinishedImmediate.connect(bind(&JobManager::job_finished, this)));
					i->start ();
				} else {
					i->resume ();
				}
				emit (boost::bind (boost::ref (ActiveJobsChanged), _last_active_job, i->json_name()));
				_last_active_job = i->json_name ();
				have_running = true;
			} else if (!have_running && i->running()) {
				have_running = true;
			}
		}

		_schedule_condition.wait(lm);
	}
}


void
JobManager::job_finished ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		emit (boost::bind(boost::ref (ActiveJobsChanged), _last_active_job, optional<string>()));
		_last_active_job = optional<string>();
	}

	_schedule_condition.notify_all();
}


JobManager *
JobManager::instance ()
{
	if (!_instance) {
		_instance = new JobManager ();
		_instance->start ();
	}

	return _instance;
}


void
JobManager::drop ()
{
	delete _instance;
	_instance = nullptr;
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

		for (auto i: _jobs) {
			auto a = dynamic_pointer_cast<AnalyseAudioJob> (i);
			if (a && a->path() == film->audio_analysis_path(playlist) && !i->finished_cancelled()) {
				i->when_finished (connection, ready);
				return;
			}
		}
	}

	shared_ptr<AnalyseAudioJob> job;

	{
		boost::mutex::scoped_lock lm (_mutex);

		job = make_shared<AnalyseAudioJob> (film, playlist, from_zero);
		connection = job->Finished.connect (ready);
		_jobs.push_back (job);
		_schedule_condition.notify_all ();
	}

	emit (boost::bind (boost::ref (JobAdded), weak_ptr<Job> (job)));
}


void
JobManager::analyse_subtitles (
	shared_ptr<const Film> film,
	shared_ptr<Content> content,
	boost::signals2::connection& connection,
	function<void()> ready
	)
{
	{
		boost::mutex::scoped_lock lm (_mutex);

		for (auto i: _jobs) {
			auto a = dynamic_pointer_cast<AnalyseSubtitlesJob> (i);
			if (a && a->path() == film->subtitle_analysis_path(content)) {
				i->when_finished (connection, ready);
				return;
			}
		}
	}

	shared_ptr<AnalyseSubtitlesJob> job;

	{
		boost::mutex::scoped_lock lm (_mutex);

		job = make_shared<AnalyseSubtitlesJob>(film, content);
		connection = job->Finished.connect (ready);
		_jobs.push_back (job);
		_schedule_condition.notify_all ();
	}

	emit (boost::bind(boost::ref(JobAdded), weak_ptr<Job>(job)));
}


void
JobManager::increase_priority (shared_ptr<Job> job)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		auto iter = std::find(_jobs.begin(), _jobs.end(), job);
		if (iter == _jobs.begin() || iter == _jobs.end()) {
			return;
		}
		swap(*iter, *std::prev(iter));
	}

	_schedule_condition.notify_all();
	emit(boost::bind(boost::ref(JobsReordered)));
}


void
JobManager::decrease_priority (shared_ptr<Job> job)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		auto iter = std::find(_jobs.begin(), _jobs.end(), job);
		if (iter == _jobs.end() || std::next(iter) == _jobs.end()) {
			return;
		}
		swap(*iter, *std::next(iter));
	}

	_schedule_condition.notify_all();
	emit(boost::bind(boost::ref(JobsReordered)));
}


/** Pause all job processing */
void
JobManager::pause ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_paused = true;
	_schedule_condition.notify_all();
}


/** Resume processing jobs after a previous pause() */
void
JobManager::resume ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_paused = false;
	_schedule_condition.notify_all();
}

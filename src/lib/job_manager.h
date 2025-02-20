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


/** @file  src/job_manager.h
 *  @brief A simple scheduler for jobs.
 */


#include "job.h"
#include "signaller.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/condition.hpp>
#include <list>


class Film;
class Playlist;
class Content;
struct threed_test7;


extern bool wait_for_jobs ();


/** @class JobManager
 *  @brief A simple scheduler for jobs.
 */
class JobManager : public Signaller
{
public:
	JobManager (JobManager const&) = delete;
	JobManager& operator= (JobManager const&) = delete;

	std::shared_ptr<Job> add (std::shared_ptr<Job>);
	std::shared_ptr<Job> add_after (std::shared_ptr<Job> after, std::shared_ptr<Job> j);
	std::list<std::shared_ptr<Job>> get () const;
	bool work_to_do () const;
	bool errors () const;
	void increase_priority (std::shared_ptr<Job>);
	void decrease_priority (std::shared_ptr<Job>);
	void pause ();
	void resume ();
	bool paused () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paused;
	}

	std::weak_ptr<Job> last_active_job() const {
		return _last_active_job;
	}

	void analyse_audio (
		std::shared_ptr<const Film> film,
		std::shared_ptr<const Playlist> playlist,
		bool from_zero,
		boost::signals2::connection& connection,
		std::function<void (Job::Result)> ready
		);

	void analyse_subtitles (
		std::shared_ptr<const Film> film,
		std::shared_ptr<Content> content,
		boost::signals2::connection& connection,
		std::function<void (Job::Result)> ready
		);

	boost::signals2::signal<void (std::weak_ptr<Job>)> JobAdded;
	boost::signals2::signal<void ()> JobsReordered;
	boost::signals2::signal<void (boost::optional<std::string>, boost::optional<std::string>)> ActiveJobsChanged;

	static JobManager* instance ();
	static void drop ();

private:
	/* This function is part of the test suite */
	friend bool ::wait_for_jobs ();
	friend struct threed_test7;

	JobManager ();
	~JobManager ();
	void scheduler ();
	void start ();
	void job_finished ();

	mutable boost::mutex _mutex;
	boost::condition _schedule_condition;
	/** List of jobs in the order that they will be executed */
	std::list<std::shared_ptr<Job>> _jobs;
	std::list<boost::signals2::connection> _connections;
	bool _terminate = false;

	std::weak_ptr<Job> _last_active_job;
	boost::thread _scheduler;

	/** true if all jobs should be paused */
	bool _paused = false;

	static JobManager* _instance;
};

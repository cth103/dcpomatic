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

/** @file  src/job_manager.h
 *  @brief A simple scheduler for jobs.
 */

#include "signaller.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>
#include <list>

class Job;
class Film;
class Playlist;

extern bool wait_for_jobs ();

/** @class JobManager
 *  @brief A simple scheduler for jobs.
 */
class JobManager : public Signaller, public boost::noncopyable
{
public:
	boost::shared_ptr<Job> add (boost::shared_ptr<Job>);
	boost::shared_ptr<Job> add_after (boost::shared_ptr<Job> after, boost::shared_ptr<Job> j);
	std::list<boost::shared_ptr<Job> > get () const;
	bool work_to_do () const;
	bool errors () const;
	void increase_priority (boost::shared_ptr<Job>);
	void decrease_priority (boost::shared_ptr<Job>);

	void analyse_audio (
		boost::shared_ptr<const Film> film,
		boost::shared_ptr<const Playlist> playlist,
		bool from_zero,
		boost::signals2::connection& connection,
		boost::function<void()> ready
		);

	boost::signals2::signal<void (boost::weak_ptr<Job>)> JobAdded;
	boost::signals2::signal<void ()> JobsReordered;
	boost::signals2::signal<void (boost::optional<std::string>, boost::optional<std::string>)> ActiveJobsChanged;

	static JobManager* instance ();
	static void drop ();

private:
	/* This function is part of the test suite */
	friend bool ::wait_for_jobs ();

	JobManager ();
	~JobManager ();
	void scheduler ();
	void start ();
	void priority_changed ();

	mutable boost::mutex _mutex;
	/** List of jobs in the order that they will be executed */
	std::list<boost::shared_ptr<Job> > _jobs;
	bool _terminate;

	boost::optional<std::string> _last_active_job;
	boost::thread* _scheduler;

	static JobManager* _instance;
};

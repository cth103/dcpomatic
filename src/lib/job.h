/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/job.h
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */

#ifndef DCPOMATIC_JOB_H
#define DCPOMATIC_JOB_H

#include "signaller.h"
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <string>

class Film;

/** @class Job
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */
class Job : public boost::enable_shared_from_this<Job>, public Signaller, public boost::noncopyable
{
public:
	Job (boost::shared_ptr<const Film>);
	virtual ~Job ();

	/** @return user-readable name of this job */
	virtual std::string name () const = 0;
	virtual std::string json_name () const = 0;
	/** Run this job in the current thread. */
	virtual void run () = 0;

	void start ();
	void pause_by_user ();
	void pause_by_priority ();
	void resume ();
	void cancel ();

	bool is_new () const;
	bool running () const;
	bool finished () const;
	bool finished_ok () const;
	bool finished_in_error () const;
	bool finished_cancelled () const;
	bool paused_by_user () const;
	bool paused_by_priority () const;

	std::string error_summary () const;
	std::string error_details () const;

	virtual std::string status () const;
	std::string json_status () const;
	std::string sub_name () const {
		return _sub_name;
	}

	void set_progress_unknown ();
	void set_progress (float, bool force = false);
	void sub (std::string);
	boost::optional<float> progress () const;

	boost::shared_ptr<const Film> film () const {
		return _film;
	}

	void when_finished (boost::signals2::connection& connection, boost::function<void()> finished);

	boost::signals2::signal<void()> Progress;
	/** Emitted from the UI thread when the job is finished */
	boost::signals2::signal<void()> Finished;

protected:

	virtual int remaining_time () const;

	/** Description of a job's state */
	enum State {
		NEW,		///< the job hasn't been started yet
		RUNNING,	///< the job is running
		PAUSED_BY_USER,	///< the job has been paused
		PAUSED_BY_PRIORITY, ///< the job has been paused
		FINISHED_OK,	///< the job has finished successfully
		FINISHED_ERROR, ///< the job has finished in error
		FINISHED_CANCELLED ///< the job was cancelled
	};

	void set_state (State);
	void set_error (std::string s, std::string d);
	int elapsed_sub_time () const;
	void check_for_interruption_or_pause ();

	boost::shared_ptr<const Film> _film;

private:

	void run_wrapper ();
	void set_progress_common (boost::optional<float> p);

	boost::thread* _thread;

	/** mutex for _state and _error */
	mutable boost::mutex _state_mutex;
	/** current state of the job */
	State _state;
	/** summary of an error that has occurred (when state == FINISHED_ERROR) */
	std::string _error_summary;
	std::string _error_details;

	/** time that this job was started */
	time_t _start_time;
	/** time that this sub-job was started */
	time_t _sub_start_time;
	std::string _sub_name;

	/** mutex for _progress and _last_progress_update */
	mutable boost::mutex _progress_mutex;
	boost::optional<float> _progress;
	boost::optional<struct timeval> _last_progress_update;

	/** condition to signal changes to pause/resume so that we know when to wake;
	    this could be a general _state_change if it made more sense.
	*/
	boost::condition_variable _pause_changed;

	int _ran_for;
};

#endif

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

/** @file src/job.h
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */

#ifndef DCPOMATIC_JOB_H
#define DCPOMATIC_JOB_H

#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

class Film;

/** @class Job
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */
class Job : public boost::enable_shared_from_this<Job>
{
public:
	Job (boost::shared_ptr<const Film>);
	virtual ~Job() {}

	/** @return user-readable name of this job */
	virtual std::string name () const = 0;
	/** Run this job in the current thread. */
	virtual void run () = 0;
	
	void start ();
	void pause ();
	void resume ();
	void cancel ();

	bool is_new () const;
	bool running () const;
	bool finished () const;
	bool finished_ok () const;
	bool finished_in_error () const;
	bool finished_cancelled () const;
	bool paused () const;

	std::string error_summary () const;
	std::string error_details () const;

	int elapsed_time () const;
	virtual std::string status () const;

	void set_progress_unknown ();
	void set_progress (float);
	void ascend ();
	void descend (float);
	float overall_progress () const;

	/** Emitted by the JobManagerView from the UI thread */
	boost::signals2::signal<void()> Finished;

protected:

	virtual int remaining_time () const;

	/** Description of a job's state */
	enum State {
		NEW,            ///< the job hasn't been started yet
		RUNNING,        ///< the job is running
		PAUSED,         ///< the job has been paused
		FINISHED_OK,    ///< the job has finished successfully
		FINISHED_ERROR, ///< the job has finished in error
		FINISHED_CANCELLED ///< the job was cancelled
	};
	
	void set_state (State);
	void set_error (std::string s, std::string d);

	boost::shared_ptr<const Film> _film;

private:

	void run_wrapper ();

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

	/** mutex for _stack and _progress_unknown */
	mutable boost::mutex _progress_mutex;

	struct Level {
		Level (float a) : allocation (a), normalised (0) {}

		float allocation;
		float normalised;
	};

	std::list<Level> _stack;

	/** true if this job's progress will always be unknown */
	bool _progress_unknown;

	int _ran_for;
};

#endif

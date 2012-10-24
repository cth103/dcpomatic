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

/** @file src/job.cc
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <libdcp/exceptions.h>
#include "job.h"
#include "util.h"

using namespace std;
using namespace boost;

/** @param s Film that we are operating on.
 */
Job::Job (shared_ptr<Film> f, shared_ptr<Job> req)
	: _film (f)
	, _required (req)
	, _state (NEW)
	, _start_time (0)
	, _progress_unknown (false)
	, _ran_for (0)
{
	descend (1);
}

/** Start the job in a separate thread, returning immediately */
void
Job::start ()
{
	set_state (RUNNING);
	_start_time = time (0);
	boost::thread (boost::bind (&Job::run_wrapper, this));
}

/** A wrapper for the ::run() method to catch exceptions */
void
Job::run_wrapper ()
{
	try {

		run ();

	} catch (libdcp::FileError& e) {
		
		set_progress (1);
		set_state (FINISHED_ERROR);
		set_error (String::compose ("%1 (%2)", e.what(), filesystem::path (e.filename()).leaf()));
		
	} catch (std::exception& e) {

		set_progress (1);
		set_state (FINISHED_ERROR);
		set_error (e.what ());

	}
}

bool
Job::is_new () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == NEW;
}

/** @return true if the job is running */
bool
Job::running () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == RUNNING;
}

/** @return true if the job has finished (either successfully or unsuccessfully) */
bool
Job::finished () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == FINISHED_OK || _state == FINISHED_ERROR;
}

/** @return true if the job has finished successfully */
bool
Job::finished_ok () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == FINISHED_OK;
}

/** @return true if the job has finished unsuccessfully */
bool
Job::finished_in_error () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == FINISHED_ERROR;
}

/** Set the state of this job.
 *  @param s New state.
 */
void
Job::set_state (State s)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	_state = s;

	if (_state == FINISHED_OK || _state == FINISHED_ERROR) {
		_ran_for = elapsed_time ();
	}
}

/** A hack to work around our lack of cross-thread
 *  signalling; this emits Finished, and listeners
 *  assume that it will be emitted in the GUI thread,
 *  so this method must be called from the GUI thread.
 */
void
Job::emit_finished ()
{
	Finished ();
}

/** @return Time (in seconds) that this job has been running */
int
Job::elapsed_time () const
{
	if (_start_time == 0) {
		return 0;
	}
	
	return time (0) - _start_time;
}

/** Set the progress of the current part of the job.
 *  @param p Progress (from 0 to 1)
 */
void
Job::set_progress (float p)
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	_progress_unknown = false;
	_stack.back().normalised = p;
}

/** @return fractional overall progress, or -1 if not known */
float
Job::overall_progress () const
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	if (_progress_unknown) {
		return -1;
	}

	float overall = 0;
	float factor = 1;
	for (list<Level>::const_iterator i = _stack.begin(); i != _stack.end(); ++i) {
		factor *= i->allocation;
		overall += i->normalised * factor;
	}

	if (overall > 1) {
		overall = 1;
	}
	
	return overall;
}

/** Ascend up one level in terms of progress reporting; see descend() */
void
Job::ascend ()
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	
	assert (!_stack.empty ());
	float const a = _stack.back().allocation;
	_stack.pop_back ();
	_stack.back().normalised += a;
}

/** Descend down one level in terms of progress reporting; e.g. if
 *  there is a task which is split up into N subtasks, each of which
 *  report their progress from 0 to 100%, call descend() before executing
 *  each subtask, and ascend() afterwards to ensure that overall progress
 *  is reported correctly.
 *
 *  @param a Fraction (from 0 to 1) of the current task to allocate to the subtask.
 */
void
Job::descend (float a)
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	_stack.push_back (Level (a));
}

/** @return Any error string that the job has generated */
string
Job::error () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _error;
}

/** Set the current error string.
 *  @param e New error string.
 */
void
Job::set_error (string e)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	_error = e;
}

/** Say that this job's progress will be unknown until further notice */
void
Job::set_progress_unknown ()
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	_progress_unknown = true;
}

/** @return Human-readable status of this job */
string
Job::status () const
{
	float const p = overall_progress ();
	int const t = elapsed_time ();
	int const r = remaining_time ();
	
	stringstream s;
	if (!finished () && p >= 0 && t > 10 && r > 0) {
		s << rint (p * 100) << "%; " << seconds_to_approximate_hms (r) << " remaining";
	} else if (!finished () && (t <= 10 || r == 0)) {
		s << rint (p * 100) << "%";
	} else if (finished_ok ()) {
		s << "OK (ran for " << seconds_to_hms (_ran_for) << ")";
	} else if (finished_in_error ()) {
		s << "Error (" << error() << ")";
	}

	return s.str ();
}

/** @return An estimate of the remaining time for this job, in seconds */
int
Job::remaining_time () const
{
	return elapsed_time() / overall_progress() - elapsed_time();
}

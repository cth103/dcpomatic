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

/** @file  src/job.cc
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */

#include "job.h"
#include "util.h"
#include "cross.h"
#include "exceptions.h"
#include "film.h"
#include "log.h"
#include "compose.hpp"
#include <dcp/exceptions.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::function;

#define LOG_ERROR_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_ERROR);
#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);

/** @param film Associated film, or 0 */
Job::Job (shared_ptr<const Film> film)
	: _film (film)
	, _thread (0)
	, _state (NEW)
	, _start_time (0)
	, _progress (0)
	, _ran_for (0)
{

}

Job::~Job ()
{
	if (_thread) {
		_thread->interrupt ();
		/* We can't use DCPOMATIC_ASSERT here as it may throw an exception */
		if (_thread->joinable ()) {
			try {
				_thread->join ();
			} catch (...) {
				/* Too late to do anything about this */
			}
		}
	}

	delete _thread;
}

/** Start the job in a separate thread, returning immediately */
void
Job::start ()
{
	set_state (RUNNING);
	_start_time = time (0);
	_thread = new boost::thread (boost::bind (&Job::run_wrapper, this));
}

/** A wrapper for the ::run() method to catch exceptions */
void
Job::run_wrapper ()
{
	try {

		run ();

	} catch (dcp::FileError& e) {

		string m = String::compose (_("An error occurred whilst handling the file %1."), boost::filesystem::path (e.filename()).leaf());

		try {
			boost::filesystem::space_info const s = boost::filesystem::space (e.filename());
			if (s.available < pow (1024, 3)) {
				m += N_("\n\n");
				m += _("The drive that the film is stored on is low in disc space.  Free some more space and try again.");
			}
		} catch (...) {

		}

		set_error (e.what(), m);
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (OpenFileError& e) {

		set_error (
			String::compose (_("Could not open %1"), e.file().string()),
			String::compose (
				_("DCP-o-matic could not open the file %1.  Perhaps it does not exist or is in an unexpected format."),
				boost::filesystem::absolute (e.file()).string()
				)
			);

		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (boost::filesystem::filesystem_error& e) {

		if (e.code() == boost::system::errc::no_such_file_or_directory) {
			set_error (
				String::compose (_("Could not open %1"), e.path1().string ()),
				String::compose (
					_("DCP-o-matic could not open the file %1.  Perhaps it does not exist or is in an unexpected format."),
					boost::filesystem::absolute (e.path1()).string()
					)
				);
		} else {
			set_error (
				e.what (),
				string (_("It is not known what caused this error.")) + "  " + REPORT_PROBLEM
				);
		}

		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (boost::thread_interrupted &) {

		set_state (FINISHED_CANCELLED);

	} catch (std::bad_alloc& e) {

		set_error (_("Out of memory"), _("There was not enough memory to do this.  If you are running a 32-bit operating system try reducing the number of encoding threads in the General tab of Preferences."));
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (std::exception& e) {

		set_error (
			e.what (),
			string (_("It is not known what caused this error.")) + "  " + REPORT_PROBLEM
			);

		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (...) {

		set_error (
			_("Unknown error"),
			string (_("It is not known what caused this error.")) + "  " + REPORT_PROBLEM
			);

		set_progress (1);
		set_state (FINISHED_ERROR);
	}
}

/** @return true if this job is new (ie has not started running) */
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
	return _state == FINISHED_OK || _state == FINISHED_ERROR || _state == FINISHED_CANCELLED;
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

bool
Job::finished_cancelled () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == FINISHED_CANCELLED;
}

bool
Job::paused () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == PAUSED;
}

/** Set the state of this job.
 *  @param s New state.
 */
void
Job::set_state (State s)
{
	bool finished = false;

	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_state = s;

		if (_state == FINISHED_OK || _state == FINISHED_ERROR || _state == FINISHED_CANCELLED) {
			_ran_for = elapsed_time ();
			finished = true;
			_sub_name.clear ();
		}
	}

	if (finished) {
		emit (boost::bind (boost::ref (Finished)));
	}
}

/** @return DCPTime (in seconds) that this sub-job has been running */
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
Job::set_progress (float p, bool force)
{
	if (!force && fabs (p - progress().get_value_or(0)) < 0.01) {
		/* Calm excessive progress reporting */
		return;
	}

	set_progress_common (p);
}

void
Job::set_progress_common (optional<float> p)
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	_progress = p;
	boost::this_thread::interruption_point ();

	boost::mutex::scoped_lock lm2 (_state_mutex);
	while (_state == PAUSED) {
		_pause_changed.wait (lm2);
	}

	lm.unlock ();
	lm2.unlock ();

	emit (boost::bind (boost::ref (Progress)));
}

/** @return fractional progress of the current sub-job, if known */
optional<float>
Job::progress () const
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	return _progress;
}

void
Job::sub (string n)
{
	{
		boost::mutex::scoped_lock lm (_progress_mutex);
		LOG_GENERAL ("Sub-job %1 starting", n);
		_sub_name = n;
	}

	set_progress (0, true);
}

string
Job::error_details () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _error_details;
}

/** @return A summary of any error that the job has generated */
string
Job::error_summary () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _error_summary;
}

/** Set the current error string.
 *  @param e New error string.
 */
void
Job::set_error (string s, string d)
{
	if (_film) {
		LOG_ERROR_NC (s);
		LOG_ERROR_NC (d);
		_film->log()->log (String::compose ("Error in job: %1 (%2)", s, d), LogEntry::TYPE_ERROR);
	}

	boost::mutex::scoped_lock lm (_state_mutex);
	_error_summary = s;
	_error_details = d;
}

/** Say that this job's progress will be unknown until further notice */
void
Job::set_progress_unknown ()
{
	set_progress_common (optional<float> ());
}

/** @return Human-readable status of this job */
string
Job::status () const
{
	optional<float> p = progress ();
	int const t = elapsed_time ();
	int const r = remaining_time ();

	SafeStringStream s;
	if (!finished () && p) {
		int pc = lrintf (p.get() * 100);
		if (pc == 100) {
			/* 100% makes it sound like we've finished when we haven't */
			pc = 99;
		}

		s << pc << N_("%");

		if (t > 10 && r > 0) {
			/// TRANSLATORS: remaining here follows an amount of time that is remaining
			/// on an operation.
			s << "; " << seconds_to_approximate_hms (r) << " " << _("remaining");
		}
	} else if (finished_ok ()) {
		s << String::compose (_("OK (ran for %1)"), seconds_to_hms (_ran_for));
	} else if (finished_in_error ()) {
		s << String::compose (_("Error: %1"), error_summary ());
	} else if (finished_cancelled ()) {
		s << _("Cancelled");
	}

	return s.str ();
}

string
Job::json_status () const
{
	boost::mutex::scoped_lock lm (_state_mutex);

	switch (_state) {
	case NEW:
		return N_("new");
	case RUNNING:
		return N_("running");
	case PAUSED:
		return N_("paused");
	case FINISHED_OK:
		return N_("finished_ok");
	case FINISHED_ERROR:
		return N_("finished_error");
	case FINISHED_CANCELLED:
		return N_("finished_cancelled");
	}

	return "";
}

/** @return An estimate of the remaining time for this sub-job, in seconds */
int
Job::remaining_time () const
{
	if (progress().get_value_or(0) == 0) {
		return elapsed_time ();
	}

	return elapsed_time() / progress().get() - elapsed_time();
}

void
Job::cancel ()
{
	if (!_thread) {
		return;
	}

	if (paused ()) {
		resume ();
	}

	_thread->interrupt ();
	DCPOMATIC_ASSERT (_thread->joinable ());
	_thread->join ();
	delete _thread;
	_thread = 0;
}

void
Job::pause ()
{
	if (running ()) {
		set_state (PAUSED);
		_pause_changed.notify_all ();
	}
}

void
Job::resume ()
{
	if (paused ()) {
		set_state (RUNNING);
		_pause_changed.notify_all ();
	}
}

void
Job::when_finished (boost::signals2::connection& connection, function<void()> finished)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	if (_state == FINISHED_OK || _state == FINISHED_ERROR || _state == FINISHED_CANCELLED) {
		finished ();
	} else {
		connection = Finished.connect (finished);
	}
}

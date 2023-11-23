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


/** @file  src/job.cc
 *  @brief A parent class to represent long-running tasks which are run in their own thread.
 */


#include "compose.hpp"
#include "constants.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "film.h"
#include "job.h"
#include "log.h"
#include "util.h"
#include <dcp/exceptions.h>
#include <sub/exceptions.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <time.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::function;
using std::list;
using std::shared_ptr;
using std::string;
using boost::optional;
using namespace dcpomatic;


/** @param film Associated film, or 0 */
Job::Job (shared_ptr<const Film> film)
	: _film (film)
	, _state (NEW)
	, _sub_start_time (0)
	, _progress (0)
	, _rate_limit_progress(true)
{

}


Job::~Job ()
{
#ifdef DCPOMATIC_DEBUG
	/* Any subclass should have called stop_thread in its destructor */
	assert (!_thread.joinable());
#endif
}


void
Job::stop_thread ()
{
	boost::this_thread::disable_interruption dis;

	_thread.interrupt ();
	try {
		_thread.join ();
	} catch (...) {}
}


/** Start the job in a separate thread, returning immediately */
void
Job::start ()
{
	set_state (RUNNING);
	_start_time = time (0);
	_sub_start_time = time (0);
	_thread = boost::thread (boost::bind(&Job::run_wrapper, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_thread.native_handle(), "job-wrapper");
#endif
}


/** A wrapper for the ::run() method to catch exceptions */
void
Job::run_wrapper ()
{
	start_of_thread (String::compose("Job-%1", json_name()));

	try {

		run ();

	} catch (dcp::FileError& e) {

		string m = String::compose(_("An error occurred whilst handling the file %1."), e.filename().filename());

		try {
			auto const s = dcp::filesystem::space(e.filename());
			if (s.available < pow (1024, 3)) {
				m += N_("\n\n");
				m += _("The drive that the film is stored on is low in disc space.  Free some more space and try again.");
			}
		} catch (...) {

		}

		set_error (e.what(), m);
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (dcp::StartCompressionError& e) {

		bool done = false;

#ifdef DCPOMATIC_WINDOWS
#if (__GNUC__ && !__x86_64__)
		/* 32-bit */
		set_error (
			_("Failed to encode the DCP."),
			_("This error has probably occurred because you are running the 32-bit version of DCP-o-matic and "
			  "trying to use too many encoding threads.  Please reduce the 'number of threads DCP-o-matic should "
			  "use' in the General tab of Preferences and try again.")
			);
		done = true;
#else
		/* 64-bit */
		if (running_32_on_64()) {
			set_error (
				_("Failed to encode the DCP."),
				_("This error has probably occurred because you are running the 32-bit version of DCP-o-matic.  Please re-install DCP-o-matic with the 64-bit installer and try again.")
				);
			done = true;
		}
#endif
#endif

		if (!done) {
			set_error (
				e.what (),
				string (_("It is not known what caused this error.")) + "  " + REPORT_PROBLEM
				);
		}

		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (OpenFileError& e) {

		set_error (
			String::compose (_("Could not open %1"), e.file().string()),
			String::compose (
				_("DCP-o-matic could not open the file %1 (%2).  Perhaps it does not exist or is in an unexpected format."),
				dcp::filesystem::absolute(e.file()).string(),
				e.what()
				)
			);

		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (boost::filesystem::filesystem_error& e) {

		if (e.code() == boost::system::errc::no_such_file_or_directory) {
			set_error (
				String::compose (_("Could not open %1"), e.path1().string ()),
				String::compose (
					_("DCP-o-matic could not open the file %1 (%2).  Perhaps it does not exist or is in an unexpected format."),
					dcp::filesystem::absolute(e.path1()).string(),
					e.what()
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
		/* The job was cancelled; there's nothing else we need to do here */
	} catch (sub::SubripError& e) {

		string extra = "Error is near:\n";
		for (auto i: e.context()) {
			extra += i + "\n";
		}

		set_error (e.what (), extra);
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (std::bad_alloc& e) {

		set_error (_("Out of memory"), _("There was not enough memory to do this.  If you are running a 32-bit operating system try reducing the number of encoding threads in the General tab of Preferences."));
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (dcp::ReadError& e) {

		set_error (e.message(), e.detail().get_value_or(""));
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (KDMError& e) {

		set_error (e.summary(), e.detail());
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (FileError& e) {

		set_error (e.what(), e.what());
		set_progress (1);
		set_state (FINISHED_ERROR);

	} catch (CPLNotFoundError& e) {

		set_error(e.what());
		set_progress(1);
		set_state(FINISHED_ERROR);

	} catch (MissingConfigurationError& e) {

		set_error(e.what());
		set_progress(1);
		set_state(FINISHED_ERROR);

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
Job::paused_by_user () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == PAUSED_BY_USER;
}


bool
Job::paused_by_priority () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _state == PAUSED_BY_PRIORITY;
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
		if (_state == s) {
			return;
		}

		_state = s;

		if (_state == FINISHED_OK || _state == FINISHED_ERROR || _state == FINISHED_CANCELLED) {
			_finish_time = time(nullptr);
			finished = true;
			_sub_name.clear ();
		}
	}

	if (finished) {
		auto const result = state_to_result(s);
		emit(boost::bind(boost::ref(Finished), result));
		FinishedImmediate(result);
	}
}


Job::Result
Job::state_to_result(State state) const
{
	switch (state) {
	case FINISHED_OK:
		return Result::RESULT_OK;
	case FINISHED_ERROR:
		return Result::RESULT_ERROR;
	case FINISHED_CANCELLED:
		return Result::RESULT_CANCELLED;
	default:
		DCPOMATIC_ASSERT(false);
	};

	DCPOMATIC_ASSERT(false);
}


/** @return DCPTime (in seconds) that this sub-job has been running */
int
Job::elapsed_sub_time () const
{
	if (_sub_start_time == 0) {
		return 0;
	}

	return time (0) - _sub_start_time;
}


/** Check to see if this job has been interrupted or paused */
void
Job::check_for_interruption_or_pause ()
{
	boost::this_thread::interruption_point ();

	boost::mutex::scoped_lock lm (_state_mutex);
	while (_state == PAUSED_BY_USER || _state == PAUSED_BY_PRIORITY) {
		emit (boost::bind (boost::ref (Progress)));
		_pause_changed.wait (lm);
	}
}


optional<float>
Job::seconds_since_last_progress_update () const
{
	boost::mutex::scoped_lock lm (_progress_mutex);
	if (!_last_progress_update) {
		return {};
	}

	struct timeval now;
	gettimeofday (&now, 0);

	return seconds(now) - seconds(*_last_progress_update);
}


/** Set the progress of the current part of the job.
 *  @param p Progress (from 0 to 1)
 *  @param force Do not ignore this update, even if it hasn't been long since the last one.
 */
void
Job::set_progress (float p, bool force)
{
	check_for_interruption_or_pause ();

	if (!force && _rate_limit_progress) {
		/* Check for excessively frequent progress reporting */
		boost::mutex::scoped_lock lm (_progress_mutex);
		struct timeval now;
		gettimeofday (&now, 0);
		if (_last_progress_update && _last_progress_update->tv_sec > 0) {
			double const elapsed = seconds(now) - seconds(*_last_progress_update);
			if (elapsed < 0.5) {
				return;
			}
		}
		_last_progress_update = now;
	}

	set_progress_common (p);
}


void
Job::set_progress_common (optional<float> p)
{
	{
		boost::mutex::scoped_lock lm (_progress_mutex);
		_progress = p;
	}

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
	_sub_start_time = time (0);
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
 *  @param s New error string.
 *  @param d New error detail string.
 */
void
Job::set_error (string s, string d)
{
	if (_film) {
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
	check_for_interruption_or_pause ();
	set_progress_common (optional<float> ());
}


/** @return Human-readable status of this job */
string
Job::status () const
{
	optional<float> p = progress ();
	int const t = elapsed_sub_time ();
	int const r = remaining_time ();

	auto day_of_week_to_string = [](boost::gregorian::greg_weekday d) -> std::string {
		switch (d.as_enum()) {
		case boost::date_time::Sunday:
			return _("Sunday");
		case boost::date_time::Monday:
			return _("Monday");
		case boost::date_time::Tuesday:
			return _("Tuesday");
		case boost::date_time::Wednesday:
			return _("Wednesday");
		case boost::date_time::Thursday:
			return _("Thursday");
		case boost::date_time::Friday:
			return _("Friday");
		case boost::date_time::Saturday:
			return _("Saturday");
		}

		return d.as_long_string();
	};

	string s;
	if (!finished () && p) {
		int pc = lrintf (p.get() * 100);
		if (pc == 100) {
			/* 100% makes it sound like we've finished when we haven't */
			pc = 99;
		}

		char buffer[64];
		snprintf (buffer, sizeof(buffer), "%d%%", pc);
		s += buffer;

		if (t > 10 && r > 0) {
			auto now = boost::posix_time::second_clock::local_time();
			auto finish = now + boost::posix_time::seconds(r);
			char finish_string[16];
			snprintf (finish_string, sizeof(finish_string), "%02d:%02d", int(finish.time_of_day().hours()), int(finish.time_of_day().minutes()));
			string day;
			if (now.date() != finish.date()) {
				/// TRANSLATORS: the %1 in this string will be filled in with a day of the week
				/// to say what day a job will finish.
				day = String::compose (_(" on %1"), day_of_week_to_string(finish.date().day_of_week()));
			}
			/// TRANSLATORS: "remaining; finishing at" here follows an amount of time that is remaining
			/// on an operation; after it is an estimated wall-clock completion time.
			s += String::compose(
				_("; %1 remaining; finishing at %2%3"),
				seconds_to_approximate_hms(r), finish_string, day
				);
		}
	} else if (finished_ok ()) {
		auto time_string = [](time_t time) {
			auto tm = localtime(&time);
			char buffer[8];
			snprintf(buffer, sizeof(buffer), "%02d:%02d", tm->tm_hour, tm->tm_min);
			return string(buffer);
		};
		auto const duration = _finish_time - _start_time;
		if (duration < 10) {
			/* It took less than 10 seconds; it doesn't seem worth saying how long it took */
			s = _("OK");
		} else if (duration < 600) {
			/* It took less than 10 minutes; it doesn't seem worth saying when it started and finished */
			s = String::compose(_("OK (ran for %1)"), seconds_to_hms(duration));
		} else {
			s = String::compose(_("OK (ran for %1 from %2 to %3)"),  seconds_to_hms(duration), time_string(_start_time), time_string(_finish_time));
		}
	} else if (finished_in_error ()) {
		s = String::compose (_("Error: %1"), error_summary ());
	} else if (finished_cancelled ()) {
		s = _("Cancelled");
	}

	return s;
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
	case PAUSED_BY_USER:
	case PAUSED_BY_PRIORITY:
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
		return elapsed_sub_time ();
	}

	return elapsed_sub_time() / progress().get() - elapsed_sub_time();
}


void
Job::cancel ()
{
	if (_thread.joinable()) {
		resume();

		_thread.interrupt ();
		_thread.join ();
	}

	set_state (FINISHED_CANCELLED);
}


/** @return true if the job was paused, false if it was not running */
bool
Job::pause_by_user ()
{
	bool paused = false;
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		/* We can set _state here directly because we have a lock and we aren't
		   setting the job to FINISHED_*
		*/
		if (_state == RUNNING) {
			paused = true;
			_state = PAUSED_BY_USER;
		}
	}

	if (paused) {
		_pause_changed.notify_all ();
	}

	return paused;
}


void
Job::pause_by_priority ()
{
	if (running ()) {
		set_state (PAUSED_BY_PRIORITY);
		_pause_changed.notify_all ();
	}
}


void
Job::resume ()
{
	if (paused_by_user() || paused_by_priority()) {
		set_state (RUNNING);
		_pause_changed.notify_all ();
	}
}


void
Job::when_finished(boost::signals2::connection& connection, function<void(Result)> finished)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	if (_state == FINISHED_OK || _state == FINISHED_ERROR || _state == FINISHED_CANCELLED) {
		finished(state_to_result(_state));
	} else {
		connection = Finished.connect (finished);
	}
}


optional<string>
Job::message () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _message;
}


void
Job::set_message (string m)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	_message = m;
}


void
Job::set_rate_limit_progress(bool rate_limit)
{
	_rate_limit_progress = rate_limit;
}


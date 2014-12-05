/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/update.h
 *  @brief UpdateChecker class.
 */

#include <curl/curl.h>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>

/** Class to check for the existance of an update for DCP-o-matic on a remote server */
class UpdateChecker
{
public:
	UpdateChecker ();
	~UpdateChecker ();

	void run ();

	enum State {
		YES,    ///< there is an update
		FAILED, ///< the check failed, so we don't know
		NO,     ///< there is no update
		NOT_RUN ///< the check has not been run (yet)
	};

	/** @return state of the checker */
	State state () {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _state;
	}
	
	/** @return the version string of the latest stable version (if _state == YES or NO) */
	std::string stable () {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _stable;
	}

	/** @return the version string of the latest test version (if _state == YES or NO) */
	std::string test () {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _test;
	}
	
	/** @return true if the last signal emission was the first */
	bool last_emit_was_first () const {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _emits == 1;
	}

	size_t write_callback (void *, size_t, size_t);

	boost::signals2::signal<void (void)> StateChanged;

	static UpdateChecker* instance ();

private:	
	static UpdateChecker* _instance;

	void set_state (State);
	void thread ();

	char* _buffer;
	int _offset;
	CURL* _curl;

	/** mutex to protect _state, _stable, _test and _emits */
	mutable boost::mutex _data_mutex;
	State _state;
	std::string _stable;
	std::string _test;
	int _emits;

	boost::thread* _thread;
	boost::mutex _process_mutex;
	boost::condition _condition;
	int _to_do;
};

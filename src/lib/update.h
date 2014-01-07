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

#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <curl/curl.h>

class UpdateChecker
{
public:
	UpdateChecker ();
	~UpdateChecker ();

	void run (bool);

	enum State {
		YES,
		FAILED,
		NO,
		NOT_RUN
	};

	State state () {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _state;
	}
	
	std::string stable () {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _stable;
	}

	/** @return true if this check was run at startup, otherwise false */
	bool startup () const {
		boost::mutex::scoped_lock lm (_data_mutex);
		return _startup;
	}

	size_t write_callback (void *, size_t, size_t);

	boost::signals2::signal<void (void)> StateChanged;

	static UpdateChecker* instance ();

private:	
	static UpdateChecker* _instance;

	void set_state (State);

	char* _buffer;
	int _offset;
	CURL* _curl;

	/** mutex to protect _state, _stable and _startup */
	mutable boost::mutex _data_mutex;
	State _state;
	std::string _stable;
	/** true if this check was run at startup, otherwise false */
	bool _startup;

	/** mutex to ensure that only one query runs at once */
	boost::mutex _single_thread_mutex;
};

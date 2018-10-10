/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifdef DCPOMATIC_VARIANT_SWAROOP

#include "monitor_checker.h"
#include "config.h"
#include "cross.h"

using boost::bind;
using boost::ref;

MonitorChecker* MonitorChecker::_instance = 0;

MonitorChecker::MonitorChecker ()
	: _thread (0)
	, _terminate (false)
	, _ok (true)
{

}

void
MonitorChecker::run ()
{
	_thread = new boost::thread (boost::bind (&MonitorChecker::thread, this));
}

MonitorChecker::~MonitorChecker ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
	}

	if (_thread) {
		/* Ideally this would be a DCPOMATIC_ASSERT(_thread->joinable()) but we
		   can't throw exceptions from a destructor.
		*/
		_thread->interrupt ();
		try {
			if (_thread->joinable ()) {
				_thread->join ();
			}
		} catch (boost::thread_interrupted& e) {
			/* No problem */
		}
	}
	delete _thread;
}

void
MonitorChecker::thread ()
{
	while (true) {
		boost::mutex::scoped_lock lm (_mutex);
		if (_terminate) {
			break;
		}

		bool const was_ok = _ok;
		_ok = Config::instance()->required_monitors().empty() || get_monitors() == Config::instance()->required_monitors();
		if (was_ok != _ok) {
			emit (bind(boost::ref(StateChanged)));
		}

		lm.unlock ();
		dcpomatic_sleep (60);
	}
}

bool
MonitorChecker::ok () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _ok;
}

MonitorChecker *
MonitorChecker::instance ()
{
	if (!_instance) {
		_instance = new MonitorChecker ();
	}

	return _instance;
}

#endif

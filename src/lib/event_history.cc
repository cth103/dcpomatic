/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "event_history.h"
#include "util.h"
#include <boost/thread/mutex.hpp>

EventHistory::EventHistory (int size)
	: _size (size)
{

}

float
EventHistory::rate () const
{
	boost::mutex::scoped_lock lock (_mutex);
	if (int (_history.size()) < _size) {
		return 0;
	}

	struct timeval now;
	gettimeofday (&now, 0);

	return _size / (seconds (now) - seconds (_history.back ()));
}

void
EventHistory::event ()
{
	boost::mutex::scoped_lock lock (_mutex);

	struct timeval tv;
	gettimeofday (&tv, 0);
	_history.push_front (tv);
	if (int (_history.size()) > _size) {
		_history.pop_back ();
	}
}

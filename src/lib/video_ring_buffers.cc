/*
    Copyright (C) 2016-2017 Carl Hetherington <cth@carlh.net>

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

#include "video_ring_buffers.h"
#include "player_video.h"
#include <boost/foreach.hpp>
#include <list>
#include <iostream>

using std::list;
using std::make_pair;
using std::cout;
using std::pair;
using boost::shared_ptr;
using boost::optional;

void
VideoRingBuffers::put (shared_ptr<PlayerVideo> frame, DCPTime time)
{
	cout << "put " << to_string(time) << "\n";
	boost::mutex::scoped_lock lm (_mutex);
	_data.push_back (make_pair (frame, time));
}

pair<shared_ptr<PlayerVideo>, DCPTime>
VideoRingBuffers::get ()
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_data.empty ()) {
		cout << "get: no data.\n";
		return make_pair(shared_ptr<PlayerVideo>(), DCPTime());
	}
	pair<shared_ptr<PlayerVideo>, DCPTime> const r = _data.front ();
	cout << "get: here we go! " << to_string(r.second) << "\n";
	_data.pop_front ();
	return r;
}

Frame
VideoRingBuffers::size () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _data.size ();
}

void
VideoRingBuffers::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_data.clear ();
}

optional<DCPTime>
VideoRingBuffers::latest () const
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_data.empty ()) {
		return optional<DCPTime> ();
	}

	return _data.back().second;
}

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
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <list>
#include <iostream>

using std::list;
using std::make_pair;
using std::cout;
using std::pair;
using std::string;
using boost::shared_ptr;
using boost::optional;
using namespace dcpomatic;

void
VideoRingBuffers::put (shared_ptr<PlayerVideo> frame, DCPTime time)
{
	boost::mutex::scoped_lock lm (_mutex);
	_data.push_back (make_pair (frame, time));
}

pair<shared_ptr<PlayerVideo>, DCPTime>
VideoRingBuffers::get ()
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_data.empty ()) {
		return make_pair(shared_ptr<PlayerVideo>(), DCPTime());
	}
	pair<shared_ptr<PlayerVideo>, DCPTime> const r = _data.front ();
	_data.pop_front ();
	return r;
}

Frame
VideoRingBuffers::size () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _data.size ();
}

bool
VideoRingBuffers::empty () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _data.empty ();
}

void
VideoRingBuffers::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_data.clear ();
}

pair<size_t, string>
VideoRingBuffers::memory_used () const
{
	boost::mutex::scoped_lock lm (_mutex);
	size_t m = 0;
	for (list<pair<shared_ptr<PlayerVideo>, DCPTime> >::const_iterator i = _data.begin(); i != _data.end(); ++i) {
		m += i->first->memory_used();
	}
	return make_pair(m, String::compose("%1 frames", _data.size()));
}

/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "shuffler.h"
#include "content_video.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include <string>
#include <iostream>


using std::make_pair;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::optional;


int const Shuffler::_max_size = 64;


struct Comparator
{
	bool operator()(Shuffler::Store const & a, Shuffler::Store const & b) {
		if (a.second.time != b.second.time) {
			return a.second.time < b.second.time;
		}
		return a.second.eyes < b.second.eyes;
	}
};


void
Shuffler::video (weak_ptr<Piece> weak_piece, ContentVideo video)
{
	LOG_DEBUG_THREE_D("Shuffler::video time={} eyes={} part={}", to_string(video.time), static_cast<int>(video.eyes), static_cast<int>(video.part));

	if (video.eyes != Eyes::LEFT && video.eyes != Eyes::RIGHT) {
		/* Pass through anything that we don't care about */
		Video (weak_piece, video);
		return;
	}

	auto piece = weak_piece.lock ();
	DCPOMATIC_ASSERT (piece);

	if (!_last && video.eyes == Eyes::LEFT) {
		LOG_DEBUG_THREE_D ("Shuffler first after clear");
		/* We haven't seen anything since the last clear() and we have some eyes-left so assume everything is OK */
		Video (weak_piece, video);
		_last = video;
		return;
	}

	_store.push_back (make_pair (weak_piece, video));
	_store.sort (Comparator());

	while (true) {

		bool const store_front_in_sequence =
			!_store.empty() &&
			_last &&
			(
				(_store.front().second.time == _last->time && _store.front().second.eyes == Eyes::RIGHT && _last->eyes == Eyes::LEFT) ||
				(_store.front().second.time > _last->time  && _store.front().second.eyes == Eyes::LEFT  && _last->eyes == Eyes::RIGHT)
				);

		if (!store_front_in_sequence) {
			string const store = _store.empty() ? "store empty" : fmt::format("store front time={} eyes={}", to_string(_store.front().second.time), static_cast<int>(_store.front().second.eyes));
			string const last = _last ? fmt::format("last time={} eyes={}", to_string(_last->time), static_cast<int>(_last->eyes)) : "no last";
			LOG_DEBUG_THREE_D("Shuffler not in sequence: {} {}", store, last);
		}

		if (!store_front_in_sequence && _store.size() <= _max_size) {
			/* store_front_in_sequence means everything is ok; otherwise if the store is getting too big just
			   start emitting things as best we can.  This can easily happen if, for example, there is only content
			   for one eye in some part of the timeline.
			*/
			break;
		}

		if (_store.size() > _max_size) {
			LOG_WARNING("Shuffler is full after receiving frame at {}; 3D sync may be incorrect.", to_string(video.time));
		}

		LOG_DEBUG_THREE_D("Shuffler emits time={} eyes={} store={}", to_string(_store.front().second.time), static_cast<int>(_store.front().second.eyes), _store.size());
		Video (_store.front().first, _store.front().second);
		_last = _store.front().second;
		_store.pop_front ();
	}
}


void
Shuffler::clear ()
{
	LOG_DEBUG_THREE_D ("Shuffler::clear");
	_store.clear ();
	_last = optional<ContentVideo>();
}


void
Shuffler::flush ()
{
	for (auto i: _store) {
		Video (i.first, i.second);
	}
}

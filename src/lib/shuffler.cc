/*
    Copyright (C) 2018-2020 Carl Hetherington <cth@carlh.net>

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
#include <boost/foreach.hpp>
#include <string>
#include <iostream>

using std::make_pair;
using std::string;
using std::weak_ptr;
using std::shared_ptr;
using boost::optional;

int const Shuffler::_max_size = 64;

struct Comparator
{
	bool operator()(Shuffler::Store const & a, Shuffler::Store const & b) {
		if (a.second.frame != b.second.frame) {
			return a.second.frame < b.second.frame;
		}
		return a.second.eyes < b.second.eyes;
	}
};

void
Shuffler::video (weak_ptr<Piece> weak_piece, ContentVideo video)
{
	LOG_DEBUG_THREED ("Shuffler::video frame=%1 eyes=%2 part=%3", video.frame, static_cast<int>(video.eyes), static_cast<int>(video.part));

	if (video.eyes != EYES_LEFT && video.eyes != EYES_RIGHT) {
		/* Pass through anything that we don't care about */
		Video (weak_piece, video);
		return;
	}

	shared_ptr<Piece> piece = weak_piece.lock ();
	DCPOMATIC_ASSERT (piece);

	if (!_last && video.eyes == EYES_LEFT) {
		LOG_DEBUG_THREED_NC ("Shuffler first after clear");
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
				(_store.front().second.frame == _last->frame       && _store.front().second.eyes == EYES_RIGHT && _last->eyes == EYES_LEFT) ||
				(_store.front().second.frame >= (_last->frame + 1) && _store.front().second.eyes == EYES_LEFT  && _last->eyes == EYES_RIGHT)
				);

		if (!store_front_in_sequence) {
			string const store = _store.empty() ? "store empty" : String::compose("store front frame=%1 eyes=%2", _store.front().second.frame, static_cast<int>(_store.front().second.eyes));
			string const last = _last ? String::compose("last frame=%1 eyes=%2", _last->frame, static_cast<int>(_last->eyes)) : "no last";
			LOG_DEBUG_THREED("Shuffler not in sequence: %1 %2", store, last);
		}

		if (!store_front_in_sequence && _store.size() <= _max_size) {
			/* store_front_in_sequence means everything is ok; otherwise if the store is getting too big just
			   start emitting things as best we can.  This can easily happen if, for example, there is only content
			   for one eye in some part of the timeline.
			*/
			break;
		}

		if (_store.size() > _max_size) {
			LOG_WARNING ("Shuffler is full after receiving frame %1; 3D sync may be incorrect.", video.frame);
		}

		LOG_DEBUG_THREED("Shuffler emits frame=%1 eyes=%2 store=%3", _store.front().second.frame, static_cast<int>(_store.front().second.eyes), _store.size());
		Video (_store.front().first, _store.front().second);
		_last = _store.front().second;
		_store.pop_front ();
	}
}

void
Shuffler::clear ()
{
	LOG_DEBUG_THREED_NC ("Shuffler::clear");
	_store.clear ();
	_last = optional<ContentVideo>();
}

void
Shuffler::flush ()
{
	BOOST_FOREACH (Store i, _store) {
		LOG_DEBUG_PLAYER("Flushing %1 from shuffler", i.second.frame);
		Video (i.first, i.second);
	}
}

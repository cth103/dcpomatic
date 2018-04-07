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

#include "shuffler.h"
#include "content_video.h"
#include "dcpomatic_assert.h"
#include <boost/foreach.hpp>
#include <iostream>

using std::make_pair;
using boost::weak_ptr;
using boost::shared_ptr;
using boost::optional;

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
	if (video.eyes != EYES_LEFT && video.eyes != EYES_RIGHT) {
		/* Pass through anything that we don't care about */
		Video (weak_piece, video);
		return;
	}

	shared_ptr<Piece> piece = weak_piece.lock ();
	DCPOMATIC_ASSERT (piece);

	if (!_last && video.eyes == EYES_LEFT) {
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
				(_store.front().second.frame == _last->frame && _store.front().second.eyes == EYES_RIGHT && _last->eyes == EYES_LEFT) ||
				(_store.front().second.frame == (_last->frame + 1) && _store.front().second.eyes == EYES_LEFT && _last->eyes == EYES_RIGHT)
				);

		if (!store_front_in_sequence && _store.size() <= 8) {
			/* store_front_in_sequence means everything is ok; otherwise if the store is getting too big just
			   start emitting things as best we can.  This can easily happen if, for example, there is only content
			   for one eye in some part of the timeline.
			*/
			break;
		}

		Video (_store.front().first, _store.front().second);
		_last = _store.front().second;
		_store.pop_front ();
	}
}

void
Shuffler::clear ()
{
	_store.clear ();
	_last = optional<ContentVideo>();
}

void
Shuffler::flush ()
{
	BOOST_FOREACH (Store i, _store) {
		Video (i.first, i.second);
	}
}

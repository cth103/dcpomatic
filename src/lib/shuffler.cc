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
	/* Something has gong wrong if our store gets too big */
	DCPOMATIC_ASSERT (_store.size() != 8);
	/* We should only ever see 3D_LEFT / 3D_RIGHT */
	DCPOMATIC_ASSERT (video.eyes == EYES_LEFT || video.eyes == EYES_RIGHT);

	shared_ptr<Piece> piece = weak_piece.lock ();
	DCPOMATIC_ASSERT (piece);

	if (!_last) {
		/* We haven't seen anything since the last clear() so assume everything is OK */
		Video (weak_piece, video);
		_last = video;
		return;
	}

	_store.push_back (make_pair (weak_piece, video));
	_store.sort (Comparator());

	while (
		!_store.empty() &&
		(
			(_store.front().second.frame == _last->frame && _store.front().second.eyes == EYES_RIGHT && _last->eyes == EYES_LEFT) ||
			(_store.front().second.frame == (_last->frame + 1) && _store.front().second.eyes == EYES_LEFT && _last->eyes == EYES_RIGHT) ||
			_store.size() > 8
			)
		) {

		Video (_store.front().first, _store.front().second);
		_last = _store.front().second;
		_store.pop_front ();
	}
}

void
Shuffler::clear ()
{
	VideoAdjuster::clear ();
	_last = optional<ContentVideo>();
}

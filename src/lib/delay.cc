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

#include "delay.h"
#include "content_video.h"
#include "dcpomatic_assert.h"
#include <boost/foreach.hpp>
#include <iostream>

using std::make_pair;
using boost::weak_ptr;
using boost::shared_ptr;
using boost::optional;

void
Delay::video (weak_ptr<Piece> weak_piece, ContentVideo video)
{
	_store.push_back (make_pair (weak_piece, video));
	/* Delay by 2 frames */
	while (_store.size() > 2) {
		Video (_store.front().first, _store.front().second);
		_store.pop_front ();
	}
}

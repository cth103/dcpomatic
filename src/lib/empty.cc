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

#include "empty.h"
#include "playlist.h"
#include "content.h"
#include "content_part.h"
#include "dcp_content.h"
#include "dcpomatic_time_coalesce.h"
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::function;

Empty::Empty (shared_ptr<const Playlist> playlist, function<shared_ptr<ContentPart> (Content *)> part)
{
	list<DCPTimePeriod> full;
	BOOST_FOREACH (shared_ptr<Content> i, playlist->content()) {
		if (part (i.get())) {
			full.push_back (DCPTimePeriod (i->position(), i->end()));
		}
	}

	_periods = subtract (DCPTimePeriod(DCPTime(), playlist->length()), coalesce(full));
}

void
Empty::set_position (DCPTime position)
{
	_position = position;

	BOOST_FOREACH (DCPTimePeriod i, _periods) {
		if (i.contains(_position)) {
			return;
		}
	}

	BOOST_FOREACH (DCPTimePeriod i, _periods) {
		if (i.from > _position) {
			_position = i.from;
			return;
		}
	}
}

DCPTimePeriod
Empty::period_at_position () const
{
	BOOST_FOREACH (DCPTimePeriod i, _periods) {
		if (i.contains(_position)) {
			return DCPTimePeriod (_position, i.to);
		}
	}

	DCPOMATIC_ASSERT (false);
}

bool
Empty::done () const
{
	BOOST_FOREACH (DCPTimePeriod i, _periods) {
		if (i.contains(_position)) {
			return false;
		}
	}

	return true;
}

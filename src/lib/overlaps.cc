/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "overlaps.h"
#include "types.h"
#include "content.h"
#include <boost/foreach.hpp>

using boost::shared_ptr;
using boost::function;

ContentList overlaps (ContentList cl, function<bool (shared_ptr<const Content>)> part, DCPTime from, DCPTime to)
{
	ContentList overlaps;
	DCPTimePeriod period (from, to);
	BOOST_FOREACH (shared_ptr<Content> i, cl) {
		if (part(i) && DCPTimePeriod(i->position(), i->end()).overlap(period)) {
			overlaps.push_back (i);
		}
	}

	return overlaps;
}

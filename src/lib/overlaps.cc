/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "overlaps.h"
#include "types.h"
#include "content.h"
#include <boost/foreach.hpp>

using boost::shared_ptr;
using boost::function;

ContentList overlaps (ContentList cl, function<shared_ptr<ContentPart> (shared_ptr<const Content>)> part, DCPTime from, DCPTime to)
{
	ContentList overlaps;
	DCPTimePeriod period (from, to);
	BOOST_FOREACH (shared_ptr<Content> i, cl) {
		if (part(i) && DCPTimePeriod(i->position(), i->end()).overlaps (period)) {
			overlaps.push_back (i);
		}
	}

	return overlaps;
}

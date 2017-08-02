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

#include "playlist.h"
#include "dcpomatic_time.h"
#include "content_part.h"
#include <list>

struct empty_test1;
struct empty_test2;
struct player_subframe_test;

class Empty
{
public:
	Empty () {}
	Empty (ContentList content, DCPTime length, boost::function<boost::shared_ptr<ContentPart> (Content *)> part);

	DCPTime position () const {
		return _position;
	}

	DCPTimePeriod period_at_position () const;

	bool done () const;

	void set_position (DCPTime amount);

private:
	friend struct ::empty_test1;
	friend struct ::empty_test2;
	friend struct ::player_subframe_test;

	std::list<DCPTimePeriod> _periods;
	DCPTime _position;
};

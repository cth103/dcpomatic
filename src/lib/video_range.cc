/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "video_range.h"


using std::string;


string
video_range_to_string(VideoRange r)
{
	switch (r) {
	case VideoRange::FULL:
		return "full";
	case VideoRange::VIDEO:
		return "video";
	default:
		DCPOMATIC_ASSERT (false);
	}
}


VideoRange
string_to_video_range(string s)
{
	if (s == "full") {
		return VideoRange::FULL;
	} else if (s == "video") {
		return VideoRange::VIDEO;
	}

	DCPOMATIC_ASSERT (false);
	return VideoRange::FULL;
}



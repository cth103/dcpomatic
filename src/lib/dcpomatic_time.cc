/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcpomatic_time.h"
#include <inttypes.h>

using std::string;

template <>
Time<ContentTimeDifferentiator, DCPTimeDifferentiator>::Time (DCPTime d, FrameRateChange f)
	: _t (llrint (d.get() * f.speed_up))
{

}

template <>
Time<DCPTimeDifferentiator, ContentTimeDifferentiator>::Time (ContentTime d, FrameRateChange f)
	: _t (llrint (d.get() / f.speed_up))
{

}

DCPTime
min (DCPTime a, DCPTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}

DCPTime
max (DCPTime a, DCPTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}

ContentTime
min (ContentTime a, ContentTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}

ContentTime
max (ContentTime a, ContentTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}

string
to_string (ContentTime t)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "[CONT %" PRId64 " %fs]", t.get(), t.seconds());
	return buffer;
}

string
to_string (DCPTime t)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs]", t.get(), t.seconds());
	return buffer;
}

string
to_string (DCPTimePeriod p)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs -> %" PRId64 " %fs]", p.from.get(), p.from.seconds(), p.to.get(), p.to.seconds());
	return buffer;
}

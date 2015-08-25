/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "dcpomatic_time.h"

using std::ostream;

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

ostream &
operator<< (ostream& s, ContentTime t)
{
	s << "[CONT " << t.get() << " " << t.seconds() << "s]";
	return s;
}

ostream &
operator<< (ostream& s, DCPTime t)
{
	s << "[DCP " << t.get() << " " << t.seconds() << "s]";
	return s;
}

bool
ContentTimePeriod::overlaps (ContentTimePeriod const & other) const
{
	return (from < other.to && to >= other.from);
}

bool
ContentTimePeriod::contains (ContentTime const & other) const
{
	return (from <= other && other < to);
}

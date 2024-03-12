/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
using namespace dcpomatic;


bool
dcpomatic::operator<=(HMSF const& a, HMSF const& b)
{
	if (a.h != b.h) {
		return a.h <= b.h;
	}

	if (a.m != b.m) {
		return a.m <= b.m;
	}

	if (a.s != b.s) {
		return a.s <= b.s;
	}

	return a.f <= b.f;
}


template <>
Time<ContentTimeDifferentiator, DCPTimeDifferentiator>::Time (DCPTime d, FrameRateChange f)
	: _t (llrint(d.get() * f.speed_up))
{

}


template <>
Time<DCPTimeDifferentiator, ContentTimeDifferentiator>::Time (ContentTime d, FrameRateChange f)
	: _t (llrint(d.get() / f.speed_up))
{

}


DCPTime
dcpomatic::min (DCPTime a, DCPTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}


DCPTime
dcpomatic::max (DCPTime a, DCPTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}


ContentTime
dcpomatic::min (ContentTime a, ContentTime b)
{
	if (a < b) {
		return a;
	}

	return b;
}


ContentTime
dcpomatic::max (ContentTime a, ContentTime b)
{
	if (a > b) {
		return a;
	}

	return b;
}


string
dcpomatic::to_string (ContentTime t)
{
	char buffer[64];
#ifdef DCPOMATIC_WINDOWS
	__mingw_snprintf (buffer, sizeof(buffer), "[CONT %" PRId64 " %fs]", t.get(), t.seconds());
#else
	snprintf (buffer, sizeof(buffer), "[CONT %" PRId64 " %fs]", t.get(), t.seconds());
#endif
	return buffer;
}


string
dcpomatic::to_string (DCPTime t)
{
	char buffer[64];
#ifdef DCPOMATIC_WINDOWS
	__mingw_snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs]", t.get(), t.seconds());
#else
	snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs]", t.get(), t.seconds());
#endif
	return buffer;
}


string
dcpomatic::to_string (DCPTimePeriod p)
{
	char buffer[64];
#ifdef DCPOMATIC_WINDOWS
	__mingw_snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs -> %" PRId64 " %fs]", p.from.get(), p.from.seconds(), p.to.get(), p.to.seconds());
#else
	snprintf (buffer, sizeof(buffer), "[DCP %" PRId64 " %fs -> %" PRId64 " %fs]", p.from.get(), p.from.seconds(), p.to.get(), p.to.seconds());
#endif
	return buffer;
}

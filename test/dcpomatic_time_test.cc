/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/test/unit_test.hpp>
#include "lib/dcpomatic_time.h"

BOOST_AUTO_TEST_CASE (dcpomatic_time_test)
{
	FrameRateChange frc (24, 48);
	int j = 0;
	int k = 0;
	for (int64_t i = 0; i < 62000; i += 2000) {
		DCPTime d (i);
		ContentTime c (d, frc);
		BOOST_CHECK_EQUAL (c.frames_floor (24.0), j);
		++k;
		if (k == 2) {
			++j;
			k = 0;
		}
	}
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_overlaps_test)
{
	/* Taking times as the start of a sampling interval

	   |--|--|--|--|--|--|--|--|--|--|
	   0  1  2  3  4  5  6  7  8  9  |
	   |--|--|--|--|--|--|--|--|--|--|

	   <------a----><----b----->

	   and saying `from' is the start of the first sampling
	   interval and `to' is the start of the interval after
	   the period... a and b do not overlap.
	*/

	TimePeriod<DCPTime> a (DCPTime (0), DCPTime (4));
	TimePeriod<DCPTime> b (DCPTime (4), DCPTime (8));
	BOOST_CHECK (!a.overlap (b));

	/* Some more obvious non-overlaps */
	a = TimePeriod<DCPTime> (DCPTime (0), DCPTime (4));
	b = TimePeriod<DCPTime> (DCPTime (5), DCPTime (8));
	BOOST_CHECK (!a.overlap (b));

	/* Some overlaps */
	a = TimePeriod<DCPTime> (DCPTime (0), DCPTime (4));
	b = TimePeriod<DCPTime> (DCPTime (3), DCPTime (8));
	BOOST_CHECK (a.overlap(b));
	BOOST_CHECK (a.overlap(b).get() == DCPTimePeriod(DCPTime(3), DCPTime(4)));
	a = TimePeriod<DCPTime> (DCPTime (1), DCPTime (9));
	b = TimePeriod<DCPTime> (DCPTime (0), DCPTime (10));
	BOOST_CHECK (a.overlap(b));
	BOOST_CHECK (a.overlap(b).get() == DCPTimePeriod(DCPTime(1), DCPTime(9)));
}

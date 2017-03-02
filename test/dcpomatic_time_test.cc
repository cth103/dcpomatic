/*
    Copyright (C) 2015-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  test/dcpomatic_time_test.cc
 *  @brief Test Time and TimePeriod classes.
 *  @ingroup selfcontained
 */

#include "lib/dcpomatic_time.h"
#include <boost/test/unit_test.hpp>
#include <list>

using std::list;

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

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test1)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (0), DCPTime (42)));
	B.push_back (DCPTimePeriod (DCPTime (52), DCPTime (91)));
	B.push_back (DCPTimePeriod (DCPTime (94), DCPTime (106)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (42));
	BOOST_CHECK (i->to == DCPTime (52));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (91));
	BOOST_CHECK (i->to == DCPTime (94));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test2)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (14), DCPTime (42)));
	B.push_back (DCPTimePeriod (DCPTime (52), DCPTime (91)));
	B.push_back (DCPTimePeriod (DCPTime (94), DCPTime (106)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (0));
	BOOST_CHECK (i->to == DCPTime (14));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (42));
	BOOST_CHECK (i->to == DCPTime (52));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (91));
	BOOST_CHECK (i->to == DCPTime (94));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test3)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (14), DCPTime (42)));
	B.push_back (DCPTimePeriod (DCPTime (52), DCPTime (91)));
	B.push_back (DCPTimePeriod (DCPTime (94), DCPTime (99)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (0));
	BOOST_CHECK (i->to == DCPTime (14));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (42));
	BOOST_CHECK (i->to == DCPTime (52));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (91));
	BOOST_CHECK (i->to == DCPTime (94));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (99));
	BOOST_CHECK (i->to == DCPTime (106));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test4)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (0));
	BOOST_CHECK (i->to == DCPTime (106));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test5)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (14), DCPTime (42)));
	B.push_back (DCPTimePeriod (DCPTime (42), DCPTime (91)));
	B.push_back (DCPTimePeriod (DCPTime (94), DCPTime (99)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (0));
	BOOST_CHECK (i->to == DCPTime (14));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from ==DCPTime (91));
	BOOST_CHECK (i->to == DCPTime (94));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (99));
	BOOST_CHECK (i->to == DCPTime (106));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test6)
{
	DCPTimePeriod A (DCPTime (0), DCPTime (106));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (0), DCPTime (42)));
	B.push_back (DCPTimePeriod (DCPTime (42), DCPTime (91)));
	B.push_back (DCPTimePeriod (DCPTime (91), DCPTime (106)));
	list<DCPTimePeriod> r = subtract (A, B);
	BOOST_CHECK (r.empty());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test7)
{
	DCPTimePeriod A (DCPTime (228), DCPTime (356));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime (34), DCPTime (162)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (i->from == DCPTime (228));
	BOOST_CHECK (i->to == DCPTime (356));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

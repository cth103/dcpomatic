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
 *  @brief Test dcpomatic::Time and dcpomatic::TimePeriod classes.
 *  @ingroup selfcontained
 */

#include "lib/dcpomatic_time.h"
#include "lib/dcpomatic_time_coalesce.h"
#include <boost/test/unit_test.hpp>
#include <list>
#include <iostream>

using std::list;
using std::cout;
using namespace dcpomatic;

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

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_subtract_test8)
{
	DCPTimePeriod A (DCPTime(0), DCPTime(32000));
	list<DCPTimePeriod> B;
	B.push_back (DCPTimePeriod (DCPTime(8000), DCPTime(20000)));
	B.push_back (DCPTimePeriod (DCPTime(28000), DCPTime(32000)));
	list<DCPTimePeriod> r = subtract (A, B);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (*i == DCPTimePeriod(DCPTime(0), DCPTime(8000)));
	++i;
	BOOST_REQUIRE (i != r.end ());
	BOOST_CHECK (*i == DCPTimePeriod(DCPTime(20000), DCPTime(28000)));
	++i;
	BOOST_REQUIRE (i == r.end ());
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_coalesce_test1)
{
	DCPTimePeriod A (DCPTime(14), DCPTime(29));
	DCPTimePeriod B (DCPTime(45), DCPTime(91));
	list<DCPTimePeriod> p;
	p.push_back (A);
	p.push_back (B);
	list<DCPTimePeriod> q = coalesce (p);
	BOOST_REQUIRE_EQUAL (q.size(), 2U);
	BOOST_CHECK (q.front() == DCPTimePeriod(DCPTime(14), DCPTime(29)));
	BOOST_CHECK (q.back () == DCPTimePeriod(DCPTime(45), DCPTime(91)));
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_coalesce_test2)
{
	DCPTimePeriod A (DCPTime(14), DCPTime(29));
	DCPTimePeriod B (DCPTime(26), DCPTime(91));
	list<DCPTimePeriod> p;
	p.push_back (A);
	p.push_back (B);
	list<DCPTimePeriod> q = coalesce (p);
	BOOST_REQUIRE_EQUAL (q.size(), 1U);
	BOOST_CHECK (q.front() == DCPTimePeriod(DCPTime(14), DCPTime(91)));
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_coalesce_test3)
{
	DCPTimePeriod A (DCPTime(14), DCPTime(29));
	DCPTimePeriod B (DCPTime(29), DCPTime(91));
	list<DCPTimePeriod> p;
	p.push_back (A);
	p.push_back (B);
	list<DCPTimePeriod> q = coalesce (p);
	BOOST_REQUIRE_EQUAL (q.size(), 1U);
	BOOST_CHECK (q.front() == DCPTimePeriod(DCPTime(14), DCPTime(91)));
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_coalesce_test4)
{
	DCPTimePeriod A (DCPTime(14), DCPTime(29));
	DCPTimePeriod B (DCPTime(20), DCPTime(91));
	DCPTimePeriod C (DCPTime(35), DCPTime(106));
	list<DCPTimePeriod> p;
	p.push_back (A);
	p.push_back (B);
	p.push_back (C);
	list<DCPTimePeriod> q = coalesce (p);
	BOOST_REQUIRE_EQUAL (q.size(), 1U);
	BOOST_CHECK (q.front() == DCPTimePeriod(DCPTime(14), DCPTime(106)));
}

BOOST_AUTO_TEST_CASE (dcpomatic_time_period_coalesce_test5)
{
	DCPTimePeriod A (DCPTime(14), DCPTime(29));
	DCPTimePeriod B (DCPTime(20), DCPTime(91));
	DCPTimePeriod C (DCPTime(100), DCPTime(106));
	list<DCPTimePeriod> p;
	p.push_back (A);
	p.push_back (B);
	p.push_back (C);
	list<DCPTimePeriod> q = coalesce (p);
	BOOST_REQUIRE_EQUAL (q.size(), 2U);
	BOOST_CHECK (q.front() == DCPTimePeriod(DCPTime(14), DCPTime(91)));
	BOOST_CHECK (q.back()  == DCPTimePeriod(DCPTime(100), DCPTime(106)));
}

/* Straightforward test of DCPTime::ceil */
BOOST_AUTO_TEST_CASE (dcpomatic_time_ceil_test)
{
	BOOST_CHECK_EQUAL (DCPTime(0).ceil(DCPTime::HZ / 2).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).ceil(DCPTime::HZ / 2).get(), 2);
	BOOST_CHECK_EQUAL (DCPTime(2).ceil(DCPTime::HZ / 2).get(), 2);
	BOOST_CHECK_EQUAL (DCPTime(3).ceil(DCPTime::HZ / 2).get(), 4);

	BOOST_CHECK_EQUAL (DCPTime(0).ceil(DCPTime::HZ / 42).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).ceil(DCPTime::HZ / 42).get(), 42);
	BOOST_CHECK_EQUAL (DCPTime(42).ceil(DCPTime::HZ / 42).get(), 42);
	BOOST_CHECK_EQUAL (DCPTime(43).ceil(DCPTime::HZ / 42).get(), 84);

	/* Check that rounding up to non-integer frame rates works */
	BOOST_CHECK_EQUAL (DCPTime(45312).ceil(29.976).get(), 48038);

	/* Check another tricky case that used to fail */
	BOOST_CHECK_EQUAL (DCPTime(212256039).ceil(23.976).get(), 212256256);
}

/* Straightforward test of DCPTime::floor */
BOOST_AUTO_TEST_CASE (dcpomatic_time_floor_test)
{
	BOOST_CHECK_EQUAL (DCPTime(0).floor(DCPTime::HZ / 2).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).floor(DCPTime::HZ / 2).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(2).floor(DCPTime::HZ / 2).get(), 2);
	BOOST_CHECK_EQUAL (DCPTime(3).floor(DCPTime::HZ / 2).get(), 2);

	BOOST_CHECK_EQUAL (DCPTime(0).floor(DCPTime::HZ / 42).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).floor(DCPTime::HZ / 42).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(42).floor(DCPTime::HZ / 42.0).get(), 42);
	BOOST_CHECK_EQUAL (DCPTime(43).floor(DCPTime::HZ / 42.0).get(), 42);

	/* Check that rounding down to non-integer frame rates works */
	BOOST_CHECK_EQUAL (DCPTime(45312).floor(29.976).get(), 44836);
}

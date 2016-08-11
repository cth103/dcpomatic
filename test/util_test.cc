/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/util_test.cc
 *  @brief Test various utility methods.
 */

#include "lib/util.h"
#include "lib/exceptions.h"
#include <boost/test/unit_test.hpp>

using std::string;
using std::vector;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (digest_head_tail_test)
{
	vector<boost::filesystem::path> p;
	p.push_back ("test/data/digest.test");
	BOOST_CHECK_EQUAL (digest_head_tail (p, 1024), "57497ef84a0487f2bb0939a1f5703912");

	p.push_back ("test/data/digest.test2");
	BOOST_CHECK_EQUAL (digest_head_tail (p, 1024), "5a3a89857b931755ae728a518224a05c");

	p.clear ();
	p.push_back ("test/data/digest.test3");
	p.push_back ("test/data/digest.test");
	p.push_back ("test/data/digest.test2");
	p.push_back ("test/data/digest.test4");
	BOOST_CHECK_EQUAL (digest_head_tail (p, 1024), "52ccf111e4e72b58bb7b2aaa6bd45ea5");

	p.clear ();
	p.push_back ("foobar");
	BOOST_CHECK_THROW (digest_head_tail (p, 1024), OpenFileError);
}

/* Straightforward test of DCPTime::round_up */
BOOST_AUTO_TEST_CASE (dcptime_round_up_test)
{
	BOOST_CHECK_EQUAL (DCPTime(0).round_up(DCPTime::HZ / 2).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).round_up(DCPTime::HZ / 2).get(), 2);
	BOOST_CHECK_EQUAL (DCPTime(2).round_up(DCPTime::HZ / 2).get(), 2);
	BOOST_CHECK_EQUAL (DCPTime(3).round_up(DCPTime::HZ / 2).get(), 4);

	BOOST_CHECK_EQUAL (DCPTime(0).round_up(DCPTime::HZ / 42).get(), 0);
	BOOST_CHECK_EQUAL (DCPTime(1).round_up(DCPTime::HZ / 42).get(), 42);
	BOOST_CHECK_EQUAL (DCPTime(42).round_up(DCPTime::HZ / 42).get(), 42);
	BOOST_CHECK_EQUAL (DCPTime(43).round_up(DCPTime::HZ / 42).get(), 84);

	/* Check that rounding up to non-integer frame rates works */
	BOOST_CHECK_EQUAL (DCPTime(45312).round_up(29.976).get(), 48045);
}

BOOST_AUTO_TEST_CASE (timecode_test)
{
	DCPTime t = DCPTime::from_seconds (2 * 60 * 60 + 4 * 60 + 31) + DCPTime::from_frames (19, 24);
	BOOST_CHECK_EQUAL (t.timecode (24), "02:04:31:19");
}

BOOST_AUTO_TEST_CASE (seconds_to_approximate_hms_test)
{
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1), "1s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (2), "2s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (60), "1m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1.5 * 60), "1m 30s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (2 * 60), "2m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (17 * 60 + 20), "17m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1 * 3600), "1h");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (3600 + 40 * 60), "1h 40m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (13 * 3600 + 40 * 60), "14h");
}

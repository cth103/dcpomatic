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

/* Straightforward test of DCPTime::ceil */
BOOST_AUTO_TEST_CASE (dcptime_ceil_test)
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
}

/* Straightforward test of DCPTime::floor */
BOOST_AUTO_TEST_CASE (dcptime_floor_test)
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

BOOST_AUTO_TEST_CASE (tidy_for_filename_test)
{
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish\\chips"), "fish_chips");
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish:chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish/chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL (tidy_for_filename ("abcdefghï"), "abcdefghï");
}

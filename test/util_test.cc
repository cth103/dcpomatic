/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup selfcontained
 */


#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include "test.h"
#include <dcp/certificate_chain.h>
#include <boost/test/unit_test.hpp>
#include <boost/bind/bind.hpp>


using std::list;
using std::string;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


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


BOOST_AUTO_TEST_CASE (timecode_test)
{
	auto t = DCPTime::from_seconds (2 * 60 * 60 + 4 * 60 + 31) + DCPTime::from_frames (19, 24);
	BOOST_CHECK_EQUAL (t.timecode (24), "02:04:31:19");
}


BOOST_AUTO_TEST_CASE (seconds_to_approximate_hms_test)
{
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(1), "1s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(2), "2s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(60), "1m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(1.5 * 60), "1m 30s");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(2 * 60), "2m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(17 * 60 + 20), "17m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(1 * 3600), "1h");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(3600 + 40 * 60), "1h 40m");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(2 * 3600), "2h");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(2 * 3600 - 1), "2h");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms(13 * 3600 + 40 * 60), "14h");
}


BOOST_AUTO_TEST_CASE (time_to_hmsf_test)
{
	BOOST_CHECK_EQUAL (time_to_hmsf(DCPTime::from_frames(12, 24), 24), "0:00:00.12");
	BOOST_CHECK_EQUAL (time_to_hmsf(DCPTime::from_frames(24, 24), 24), "0:00:01.0");
	BOOST_CHECK_EQUAL (time_to_hmsf(DCPTime::from_frames(32, 24), 24), "0:00:01.8");
	BOOST_CHECK_EQUAL (time_to_hmsf(DCPTime::from_seconds(92), 24), "0:01:32.0");
	BOOST_CHECK_EQUAL (time_to_hmsf(DCPTime::from_seconds(2 * 60 * 60 + 92), 24), "2:01:32.0");
}


BOOST_AUTO_TEST_CASE (tidy_for_filename_test)
{
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish\\chips"), "fish_chips");
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish:chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL (tidy_for_filename ("fish/chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL (tidy_for_filename ("abcdefghï"), "abcdefghï");
}


BOOST_AUTO_TEST_CASE (utf8_strlen_test)
{
	BOOST_CHECK_EQUAL (utf8_strlen("hello world"), 11U);
	BOOST_CHECK_EQUAL (utf8_strlen("hëllo world"), 11U);
	BOOST_CHECK_EQUAL (utf8_strlen("hëłlo wørld"), 11U);
}


BOOST_AUTO_TEST_CASE (careful_string_filter_test)
{
	BOOST_CHECK_EQUAL ("hello_world", careful_string_filter("hello_world"));
	BOOST_CHECK_EQUAL ("hello_world", careful_string_filter("héllo_world"));
	BOOST_CHECK_EQUAL ("hello_world", careful_string_filter("héllo_wörld"));
	BOOST_CHECK_EQUAL ("hello_world", careful_string_filter("héllo_wörld"));
	BOOST_CHECK_EQUAL ("hello_world_a", careful_string_filter("héllo_wörld_à"));
	BOOST_CHECK_EQUAL ("hello_world_CcGgIOoSsUuLl", careful_string_filter("hello_world_ÇçĞğİÖöŞşÜüŁł"));
}


static list<float> progress_values;

static void
progress (float p)
{
	progress_values.push_back (p);
}


BOOST_AUTO_TEST_CASE (copy_in_bits_test)
{
	for (int i = 0; i < 32; ++i) {
		make_random_file ("build/test/random.dat", std::max(1, rand() % (256 * 1024 * 1024)));

		progress_values.clear ();
		copy_in_bits ("build/test/random.dat", "build/test/random.dat2", boost::bind(&progress, _1));
		BOOST_CHECK (!progress_values.empty());

		check_file ("build/test/random.dat", "build/test/random.dat2");
	}
}

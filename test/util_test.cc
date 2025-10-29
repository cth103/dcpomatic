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
#include <fmt/format.h>
#include <boost/bind/bind.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/test/unit_test.hpp>



using std::list;
using std::string;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE(digest_head_tail_test)
{
	vector<boost::filesystem::path> p;
	p.push_back("test/data/digest.test");
	BOOST_CHECK_EQUAL(digest_head_tail(p, 1024), "57497ef84a0487f2bb0939a1f5703912");

	p.push_back("test/data/digest.test2");
	BOOST_CHECK_EQUAL(digest_head_tail(p, 1024), "5a3a89857b931755ae728a518224a05c");

	p.clear();
	p.push_back("test/data/digest.test3");
	p.push_back("test/data/digest.test");
	p.push_back("test/data/digest.test2");
	p.push_back("test/data/digest.test4");
	BOOST_CHECK_EQUAL(digest_head_tail(p, 1024), "52ccf111e4e72b58bb7b2aaa6bd45ea5");

	p.clear();
	p.push_back("foobar");
	BOOST_CHECK_THROW(digest_head_tail(p, 1024), OpenFileError);
}


BOOST_AUTO_TEST_CASE(timecode_test)
{
	auto t = DCPTime::from_seconds(2 * 60 * 60 + 4 * 60 + 31) + DCPTime::from_frames(19, 24);
	BOOST_CHECK_EQUAL(t.timecode(24), "02:04:31:19");
}


BOOST_AUTO_TEST_CASE(seconds_to_approximate_hms_test)
{
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(1), "1s");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(2), "2s");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(60), "1m");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(1.5 * 60), "1m 30s");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(2 * 60), "2m");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(17 * 60 + 20), "17m");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(1 * 3600), "1h");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(3600 + 40 * 60), "1h 40m");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(2 * 3600), "2h");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(2 * 3600 - 1), "2h");
	BOOST_CHECK_EQUAL(seconds_to_approximate_hms(13 * 3600 + 40 * 60), "14h");
}


BOOST_AUTO_TEST_CASE(time_to_hmsf_test)
{
	BOOST_CHECK_EQUAL(time_to_hmsf(DCPTime::from_frames(12, 24), 24), "00:00:00.12");
	BOOST_CHECK_EQUAL(time_to_hmsf(DCPTime::from_frames(24, 24), 24), "00:00:01.00");
	BOOST_CHECK_EQUAL(time_to_hmsf(DCPTime::from_frames(32, 24), 24), "00:00:01.08");
	BOOST_CHECK_EQUAL(time_to_hmsf(DCPTime::from_seconds(92), 24), "00:01:32.00");
	BOOST_CHECK_EQUAL(time_to_hmsf(DCPTime::from_seconds(2 * 60 * 60 + 92), 24), "02:01:32.00");
}


BOOST_AUTO_TEST_CASE(tidy_for_filename_test)
{
	BOOST_CHECK_EQUAL(tidy_for_filename("fish\\chips"), "fish_chips");
	BOOST_CHECK_EQUAL(tidy_for_filename("fish:chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL(tidy_for_filename("fish/chips\\"), "fish_chips_");
	BOOST_CHECK_EQUAL(tidy_for_filename("abcdefghï"), "abcdefghï");
}


BOOST_AUTO_TEST_CASE(utf8_strlen_test)
{
	BOOST_CHECK_EQUAL(utf8_strlen("hello world"), 11U);
	BOOST_CHECK_EQUAL(utf8_strlen("hëllo world"), 11U);
	BOOST_CHECK_EQUAL(utf8_strlen("hëłlo wørld"), 11U);
}


BOOST_AUTO_TEST_CASE(careful_string_filter_test)
{
	BOOST_CHECK_EQUAL("hello_world", careful_string_filter("hello_world"));
	BOOST_CHECK_EQUAL("hello_world", careful_string_filter("héllo_world"));
	BOOST_CHECK_EQUAL("hello_world", careful_string_filter("héllo_wörld"));
	BOOST_CHECK_EQUAL("hello_world", careful_string_filter("héllo_wörld"));
	BOOST_CHECK_EQUAL("hello_world_a", careful_string_filter("héllo_wörld_à"));
	BOOST_CHECK_EQUAL("hello_world_CcGgIOoSsUuLl", careful_string_filter("hello_world_ÇçĞğİÖöŞşÜüŁł"));
}


static list<float> progress_values;

static void
progress(float p)
{
	progress_values.push_back(p);
}


BOOST_AUTO_TEST_CASE(copy_in_bits_test)
{
	for (int i = 0; i < 32; ++i) {
		make_random_file("build/test/random.dat", std::max(1, rand() % (256 * 1024 * 1024)));

		progress_values.clear();
		copy_in_bits("build/test/random.dat", "build/test/random.dat2", boost::bind(&progress, _1));
		BOOST_CHECK(!progress_values.empty());

		check_file("build/test/random.dat", "build/test/random.dat2");
	}
}


BOOST_AUTO_TEST_CASE(word_wrap_test)
{
	BOOST_CHECK_EQUAL(word_wrap("hello world", 8), "hello \nworld\n");
	BOOST_CHECK(word_wrap("hello this is a longer bit of text and it should be word-wrapped", 31) == string{"hello this is a longer bit of \ntext and it should be word-\nwrapped\n"});
	BOOST_CHECK_EQUAL(word_wrap("hellocan'twrapthissadly", 5), "hello\ncan't\nwrapt\nhissa\ndly\n");
}


BOOST_AUTO_TEST_CASE(screen_names_to_string_test)
{
	BOOST_CHECK_EQUAL(screen_names_to_string({"1", "2", "3"}), "1, 2, 3");
	BOOST_CHECK_EQUAL(screen_names_to_string({"3", "2", "1"}), "1, 2, 3");
	BOOST_CHECK_EQUAL(screen_names_to_string({"39", "3", "10", "1", "2"}), "1, 2, 3, 10, 39");
	BOOST_CHECK_EQUAL(screen_names_to_string({"Sheila", "Fred", "Jim"}), "Fred, Jim, Sheila");
	BOOST_CHECK_EQUAL(screen_names_to_string({"Sheila", "Fred", "Jim", "1"}), "1, Fred, Jim, Sheila");
}


BOOST_AUTO_TEST_CASE(rfc_2822_date_test)
{
#ifdef DCPOMATIC_WINDOWS
	auto result = setlocale(LC_TIME, "German");
#endif
#ifdef DCPOMATIC_OSX
	auto result = setlocale(LC_TIME, "de_DE");
#endif
#ifdef DCPOMATIC_LINUX
	auto result = setlocale(LC_TIME, "de_DE.UTF8");
#endif
	BOOST_REQUIRE(result);

	auto const utc_now = boost::posix_time::second_clock::universal_time();
	auto const local_now = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(utc_now);
	auto const offset = local_now - utc_now;

	auto const hours = int(abs(offset.hours()));
	auto const tz = fmt::format("{}{:02d}{:02d}", offset.hours() >= 0 ? "+" : "-", hours, int(offset.minutes()));

	int constexpr day = 24 * 60 * 60;

	/* This won't pass when running in all time zones, but it's really the overall format (and in particular
	 * the use of English for day and month names) that we want to check.
	 *
	 * On Windows using localtime (as rfc_2822_date does) to convert UTC midnight in summer 1970 to German
	 * time seems to take DST into account, giving 02:00.  On Linux the rfc_2822_date call below always gives
	 * us 01:00, even if we're trying to convert a time that was in summer.
	 *
	 * This means that we get:
	 *
	 * OS       DST now    DST in 1970  Time in 1970    tz     Check for
	 * -----------------------------------------------------------------
	 * Windows  no         no           01:00           01:00  hours
	 *          yes        no           01:00           02:00  hours - 1
	 *          no         yes          02:00           01:00  hours + 1
	 *          yes        yes          02:00           02:00  hours
	 * POSIX    no         no           01:00           01:00  hours
	 *          yes        no           01:00           02:00  hours - 1
	 *          no         yes          01:00           01:00  hours
	 *          yes        yes          01:00           02:00  hours - 1
	 */

	auto check_allowing_dst = [hours, tz](int day_index, string format) {
		auto test = rfc_2822_date(day_index * day);
		BOOST_CHECK_MESSAGE(
			test == fmt::format(format, hours + 1, tz) ||
			test == fmt::format(format, hours, tz) ||
			test == fmt::format(format, hours - 1, tz),
			test << " did not match " << fmt::format(format, hours + 1, tz) << " or " << fmt::format(format, hours, tz) << " or " << fmt::format(format, hours - 1, tz)
		);
	};

	check_allowing_dst(0, "Thu, 01 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(1, "Fri, 02 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(2, "Sat, 03 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(3, "Sun, 04 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(4, "Mon, 05 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(5, "Tue, 06 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(6, "Wed, 07 Jan 1970 {:02d}:00:00 {}");
	check_allowing_dst(39, "Mon, 09 Feb 1970 {:02d}:00:00 {}");
	check_allowing_dst(89, "Tue, 31 Mar 1970 {:02d}:00:00 {}");
	check_allowing_dst(109, "Mon, 20 Apr 1970 {:02d}:00:00 {}");
	check_allowing_dst(134, "Fri, 15 May 1970 {:02d}:00:00 {}");
	check_allowing_dst(158, "Mon, 08 Jun 1970 {:02d}:00:00 {}");
	check_allowing_dst(182, "Thu, 02 Jul 1970 {:02d}:00:00 {}");
	check_allowing_dst(221, "Mon, 10 Aug 1970 {:02d}:00:00 {}");
	check_allowing_dst(247, "Sat, 05 Sep 1970 {:02d}:00:00 {}");
	check_allowing_dst(300, "Wed, 28 Oct 1970 {:02d}:00:00 {}");
	check_allowing_dst(314, "Wed, 11 Nov 1970 {:02d}:00:00 {}");
	check_allowing_dst(363, "Wed, 30 Dec 1970 {:02d}:00:00 {}");
}


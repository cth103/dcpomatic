/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <boost/test/unit_test.hpp>
#include "lib/util.h"
#include "lib/exceptions.h"

using std::string;
using std::vector;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (split_at_spaces_considering_quotes_test)
{
	string t = "Hello this is a string \"with quotes\" and indeed without them";
	vector<string> b = split_at_spaces_considering_quotes (t);
	vector<string>::iterator i = b.begin ();
	BOOST_CHECK_EQUAL (*i++, "Hello");
	BOOST_CHECK_EQUAL (*i++, "this");
	BOOST_CHECK_EQUAL (*i++, "is");
	BOOST_CHECK_EQUAL (*i++, "a");
	BOOST_CHECK_EQUAL (*i++, "string");
	BOOST_CHECK_EQUAL (*i++, "with quotes");
	BOOST_CHECK_EQUAL (*i++, "and");
	BOOST_CHECK_EQUAL (*i++, "indeed");
	BOOST_CHECK_EQUAL (*i++, "without");
	BOOST_CHECK_EQUAL (*i++, "them");
}

BOOST_AUTO_TEST_CASE (md5_digest_test)
{
	vector<boost::filesystem::path> p;
	p.push_back ("test/data/md5.test");
	string const t = md5_digest (p, shared_ptr<Job> ());
	BOOST_CHECK_EQUAL (t, "15058685ba99decdc4398c7634796eb0");

	p.clear ();
	p.push_back ("foobar");
	BOOST_CHECK_THROW (md5_digest (p, shared_ptr<Job> ()), std::runtime_error);
}

BOOST_AUTO_TEST_CASE (divide_with_round_test)
{
	BOOST_CHECK_EQUAL (divide_with_round (0, 4), 0);
	BOOST_CHECK_EQUAL (divide_with_round (1, 4), 0);
	BOOST_CHECK_EQUAL (divide_with_round (2, 4), 1);
	BOOST_CHECK_EQUAL (divide_with_round (3, 4), 1);
	BOOST_CHECK_EQUAL (divide_with_round (4, 4), 1);
	BOOST_CHECK_EQUAL (divide_with_round (5, 4), 1);
	BOOST_CHECK_EQUAL (divide_with_round (6, 4), 2);

	BOOST_CHECK_EQUAL (divide_with_round (1000, 500), 2);
}

BOOST_AUTO_TEST_CASE (seconds_to_approximate_hms_test)
{
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1), "1 second");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (2), "2 seconds");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (60), "1 minute");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1.5 * 60), "1 minute 30 seconds");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (2 * 60), "2 minutes");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (17 * 60 + 20), "17 minutes");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (1 * 3600), "1 hour");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (3600 + 40 * 60), "1 hour 40 minutes");
	BOOST_CHECK_EQUAL (seconds_to_approximate_hms (13 * 3600 + 40 * 60), "14 hours");
}

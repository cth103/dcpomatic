/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


/** @file  test/unzipper_test.cc
 *  @brief Test Unzipper class.
 *  @ingroup selfcontained
 */


#include "lib/exceptions.h"
#include "lib/unzipper.h"
#include "lib/zipper.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <dcp/util.h>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>


using std::string;


/** Basic test of Unzipper working normally */
BOOST_AUTO_TEST_CASE(unzipper_test1)
{
	boost::system::error_code ec;
	boost::filesystem::remove("build/test/zipped.zip", ec);

	Zipper zipper("build/test/zipped.zip");
	zipper.add("foo.txt", "1234567890");
	zipper.add("bar.txt", "xxxxxxCCCCbbbbbbb1");
	zipper.add("its_bigger_than_that_chris_its_large.txt", string(128 * 1024, 'X'));
	zipper.close();

	Unzipper unzipper("build/test/zipped.zip");
	BOOST_CHECK_EQUAL(unzipper.get("foo.txt"), "1234567890");
	BOOST_CHECK_EQUAL(unzipper.get("bar.txt"), "xxxxxxCCCCbbbbbbb1");
	BOOST_CHECK_THROW(unzipper.get("hatstand"), std::runtime_error);
	BOOST_CHECK_THROW(unzipper.get("its_bigger_than_that_chris_its_large.txt"), std::runtime_error);
}


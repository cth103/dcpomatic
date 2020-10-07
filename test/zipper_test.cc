/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

/** @file  test/zipper_test.cc
 *  @brief Tests of Zipper class.
 *  @ingroup selfcontained
 */

#include "lib/zipper.h"
#include "lib/exceptions.h"
#include "test.h"
#include <dcp/util.h>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

/** Basic test of Zipper working normally */
BOOST_AUTO_TEST_CASE (zipper_test1)
{
	boost::system::error_code ec;
	boost::filesystem::remove ("build/test/zipped.zip", ec);

	Zipper zipper ("build/test/zipped.zip");
	zipper.add ("foo.txt", "1234567890");
	zipper.add ("bar.txt", "xxxxxxCCCCbbbbbbb1");
	zipper.close ();

	boost::filesystem::remove_all ("build/test/zipper_test1", ec);
	int const r = system ("unzip build/test/zipped.zip -d build/test/zipper_test1");
	BOOST_REQUIRE_EQUAL (r, 0);

	BOOST_CHECK_EQUAL (dcp::file_to_string("build/test/zipper_test1/foo.txt"), "1234567890");
	BOOST_CHECK_EQUAL (dcp::file_to_string("build/test/zipper_test1/bar.txt"), "xxxxxxCCCCbbbbbbb1");
}


/** Test failure when trying to overwrite a file */
BOOST_AUTO_TEST_CASE (zipper_test2, * boost::unit_test::depends_on("zipper_test1"))
{
	BOOST_CHECK_THROW (Zipper("build/test/zipped.zip"), FileError);
}


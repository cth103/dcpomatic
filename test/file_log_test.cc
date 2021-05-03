/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/file_log_test.cc
 *  @brief Test FileLog.
 *  @ingroup selfcontained
 */


#include "lib/file_log.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (file_log_test)
{
	FileLog log ("test/data/short.log");
	BOOST_CHECK_EQUAL (log.head_and_tail(1024), "This is a short log.\nWith only two lines.\n");
	BOOST_CHECK_EQUAL (log.head_and_tail(8), "This is \n .\n .\n .\no lines.\n");
}

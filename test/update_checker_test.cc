/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/update_checker_test.cc
 *  @brief Check UpdateChecker::version_less_than
 *  @ingroup selfcontained
 */

#include <boost/test/unit_test.hpp>
#include "lib/update_checker.h"

BOOST_AUTO_TEST_CASE (update_checker_test)
{
	BOOST_CHECK (UpdateChecker::version_less_than ("0.0.1", "0.0.2"));
	BOOST_CHECK (UpdateChecker::version_less_than ("1.0.1", "2.0.2"));
	BOOST_CHECK (UpdateChecker::version_less_than ("0.1.1", "1.5.2"));
	BOOST_CHECK (UpdateChecker::version_less_than ("1.9.45", "1.9.46"));

	BOOST_CHECK (!UpdateChecker::version_less_than ("0.0.1", "0.0.1"));
	BOOST_CHECK (!UpdateChecker::version_less_than ("2.0.2", "1.0.1"));
	BOOST_CHECK (!UpdateChecker::version_less_than ("1.5.2", "0.1.1"));
	BOOST_CHECK (!UpdateChecker::version_less_than ("1.9.46", "1.9.45"));

	BOOST_CHECK (!UpdateChecker::version_less_than ("1.9.46devel", "1.9.46"));
}

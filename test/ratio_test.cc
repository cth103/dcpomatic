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

/** @file  test/ratio_test.cc
 *  @brief Test Ratio class and fit_ratio_within().
 *  @ingroup selfcontained
 */

#include <iostream>
#include <boost/test/unit_test.hpp>
#include <dcp/util.h>
#include "lib/ratio.h"
#include "lib/util.h"
#include "test.h"

using std::ostream;

BOOST_AUTO_TEST_CASE (ratio_test)
{
	Ratio const * r = Ratio::from_id ("119");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1290, 1080));

	r = Ratio::from_id ("133");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1440, 1080));

	r = Ratio::from_id ("138");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1485, 1080));

	r = Ratio::from_id ("166");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1800, 1080));

	r = Ratio::from_id ("178");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1920, 1080));

	r = Ratio::from_id ("185");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (1998, 1080));

	r = Ratio::from_id ("239");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (2048, 858));

	r = Ratio::from_id ("190");
	BOOST_CHECK (r);
	BOOST_CHECK_EQUAL (fit_ratio_within (r->ratio(), dcp::Size (2048, 1080)), dcp::Size (2048, 1080));
}


BOOST_AUTO_TEST_CASE (ratios_use_same_pointers_test)
{
	auto const test = Ratio::from_id ("119");
	BOOST_CHECK_EQUAL (test, Ratio::from_id("119"));
}

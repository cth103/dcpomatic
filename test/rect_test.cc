/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/rect_test.cc
 *  @brief Test Rect class.
 *  @ingroup selfcontained
 */

#include "lib/rect.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using boost::optional;

BOOST_AUTO_TEST_CASE (rect_test)
{
	dcpomatic::Rect<int> a (0, 0, 100, 100);
	dcpomatic::Rect<int> b (200, 200, 100, 100);
	optional<dcpomatic::Rect<int> > c = a.intersection (b);
	BOOST_CHECK (!c);
}

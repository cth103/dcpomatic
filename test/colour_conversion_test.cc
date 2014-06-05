/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include <libdcp/colour_matrix.h>
#include "lib/colour_conversion.h"

using std::cout;

/* Basic test of identifier() for ColourConversion (i.e. a hash of the numbers) */
BOOST_AUTO_TEST_CASE (colour_conversion_test)
{
	ColourConversion A (2.4, true, libdcp::colour_matrix::srgb_to_xyz, 2.6);
	ColourConversion B (2.4, false, libdcp::colour_matrix::srgb_to_xyz, 2.6);

	BOOST_CHECK_EQUAL (A.identifier(), "1e720d2d99add654d7816f3b72da815e");
	BOOST_CHECK_EQUAL (B.identifier(), "18751a247b22682b725bf9c4caf71522");
}

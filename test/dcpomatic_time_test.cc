/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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
#include "lib/dcpomatic_time.h"

BOOST_AUTO_TEST_CASE (dcpomatic_time_test)
{
	FrameRateChange frc (24, 48);
	int j = 0;
	int k = 0;
	for (int64_t i = 0; i < 62000; i += 2000) {
		DCPTime d (i);
		ContentTime c (d, frc);
		BOOST_CHECK_EQUAL (c.frames_floor (24.0), j);
		++k;
		if (k == 2) {
			++j;
			k = 0;
		}
	}
}

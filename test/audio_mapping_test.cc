/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_mapping_test.cc
 *  @brief Basic tests of the AudioMapping class, which itself doesn't really do much.
 */

#include <boost/test/unit_test.hpp>
#include "lib/audio_mapping.h"
#include "lib/util.h"

BOOST_AUTO_TEST_CASE (audio_mapping_test)
{
	AudioMapping none;
	BOOST_CHECK_EQUAL (none.input_channels(), 0);

	AudioMapping four (4, MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL (four.input_channels(), 4);

	four.set (0, 1, 1);
	BOOST_CHECK_EQUAL (four.get (0, 1), 1);
}

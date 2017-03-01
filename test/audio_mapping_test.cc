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

/** @file  test/audio_mapping_test.cc
 *  @brief Test AudioMapping class.
 *  @ingroup selfcontained
 */

#include <boost/test/unit_test.hpp>
#include "lib/audio_mapping.h"
#include "lib/util.h"

using std::list;

BOOST_AUTO_TEST_CASE (audio_mapping_test)
{
	AudioMapping none;
	BOOST_CHECK_EQUAL (none.input_channels(), 0);

	AudioMapping four (4, MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL (four.input_channels(), 4);

	four.set (0, 1, 1);

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < MAX_DCP_AUDIO_CHANNELS; ++j) {
			BOOST_CHECK_EQUAL (four.get (i, j), (i == 0 && j == 1) ? 1 : 0);
		}
	}

	list<int> mapped = four.mapped_output_channels ();
	BOOST_CHECK_EQUAL (mapped.size(), 1);
	BOOST_CHECK_EQUAL (mapped.front(), 1);

	four.make_zero ();

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < MAX_DCP_AUDIO_CHANNELS; ++j) {
			BOOST_CHECK_EQUAL (four.get (i, j), 0);
		}
	}
}

/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_processor_delay_test.cc
 *  @brief Test the AudioDelay class.
 *  @ingroup selfcontained
 */

#include "lib/audio_delay.h"
#include "lib/audio_buffers.h"
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <iostream>

using std::cerr;
using std::cout;
using boost::shared_ptr;

#define CHECK_SAMPLE(c,f,r) \
	if (fabs(out->data(c)[f] - (r)) > 0.1) {			\
		cerr << "Sample " << f << " at line " << __LINE__ << " is " << out->data(c)[f] << " not " << r << "; difference is " << fabs(out->data(c)[f] - (r)) << "\n"; \
		BOOST_REQUIRE (fabs(out->data(c)[f] - (r)) <= 0.1);	\
	}

/** Block size greater than delay */
BOOST_AUTO_TEST_CASE (audio_processor_delay_test1)
{
	AudioDelay delay (64);

	int const C = 2;

	shared_ptr<AudioBuffers> in (new AudioBuffers (C, 256));
	for (int i = 0; i < C; ++i) {
		for (int j = 0; j < 256; ++j) {
			in->data(i)[j] = j;
		}
	}

	shared_ptr<AudioBuffers> out = delay.run (in);
	BOOST_REQUIRE_EQUAL (out->frames(), in->frames());

	/* Silence at the start */
	for (int i = 0; i < C; ++i) {
		for (int j = 0; j < 64; ++j) {
			CHECK_SAMPLE (i, j, 0);
		}
	}

	/* Then the delayed data */
	for (int i = 0; i < C; ++i) {
		for (int j = 64; j < 256; ++j) {
			CHECK_SAMPLE (i, j, j - 64);
		}
	}

	/* Feed some more in */
	for (int i = 0; i < C; ++i) {
		for (int j = 0; j < 256; ++j) {
			in->data(i)[j] = j + 256;
		}
	}
	out = delay.run (in);

	/* Check again */
	for (int i = 0; i < C; ++i) {
		for (int j = 256; j < 512; ++j) {
			CHECK_SAMPLE (i, j - 256, j - 64);
		}
	}
}

/** Block size less than delay */
BOOST_AUTO_TEST_CASE (audio_processor_delay_test2)
{
	AudioDelay delay (256);

	int const C = 2;

	/* Feeding 4 blocks of 64 should give silence each time */

	for (int i = 0; i < 4; ++i) {
		shared_ptr<AudioBuffers> in (new AudioBuffers (C, 64));
		for (int j = 0; j < C; ++j) {
			for (int k = 0; k < 64; ++k) {
				in->data(j)[k] = k + i * 64;
			}
		}

		shared_ptr<AudioBuffers> out = delay.run (in);
		BOOST_REQUIRE_EQUAL (out->frames(), in->frames());

		/* Check for silence */
		for (int j = 0; j < C; ++j) {
			for (int k = 0; k < 64; ++k) {
				CHECK_SAMPLE (j, k, 0);
			}
		}
	}

	/* Now feed 4 blocks of silence and we should see the data */
	for (int i = 0; i < 4; ++i) {
		/* Feed some silence */
		shared_ptr<AudioBuffers> in (new AudioBuffers (C, 64));
		in->make_silent ();
		shared_ptr<AudioBuffers> out = delay.run (in);

		/* Should now see the delayed data */
		for (int j = 0; j < C; ++j) {
			for (int k = 0; k < 64; ++k) {
				CHECK_SAMPLE (j, k, k + i * 64);
			}
		}
	}
}

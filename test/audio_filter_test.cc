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


/** @file  test/audio_filter_test.cc
 *  @brief Test AudioFilter, LowPassAudioFilter, HighPassAudioFilter classes.
 *  @ingroup selfcontained
 */


#include <boost/test/unit_test.hpp>
#include "lib/audio_filter.h"
#include "lib/audio_buffers.h"


using std::make_shared;
using std::shared_ptr;


static void
audio_filter_impulse_test_one (AudioFilter& f, int block_size, int num_blocks)
{
	int c = 0;

	for (int i = 0; i < num_blocks; ++i) {

		auto in = make_shared<AudioBuffers>(1, block_size);
		for (int j = 0; j < block_size; ++j) {
			in->data()[0][j] = c + j;
		}

		auto out = f.run (in);

		for (int j = 0; j < out->frames(); ++j) {
			BOOST_CHECK_EQUAL (out->data()[0][j], c + j);
		}

		c += block_size;
	}
}


/** Create a filter with an impulse as a kernel and check that it
 *  passes data through unaltered.
 */
BOOST_AUTO_TEST_CASE (audio_filter_impulse_kernel_test)
{
	AudioFilter f (0.02);

	f._ir.resize(f._M + 1);
	f._ir[0] = 1;
	for (int i = 1; i <= f._M; ++i) {
		f._ir[i] = 0;
	}

	audio_filter_impulse_test_one (f, 32, 1);
	audio_filter_impulse_test_one (f, 256, 1);
	audio_filter_impulse_test_one (f, 2048, 1);
}


/** Create filters and pass them impulses as input and check that
 *  the filter kernels comes back.
 */
BOOST_AUTO_TEST_CASE (audio_filter_impulse_input_test)
{
	LowPassAudioFilter lpf (0.02, 0.3);

	auto in = make_shared<AudioBuffers>(1, 1751);
	in->make_silent ();
	in->data(0)[0] = 1;

	auto out = lpf.run (in);
	for (int j = 0; j < out->frames(); ++j) {
		if (j <= lpf._M) {
			BOOST_CHECK_EQUAL (out->data(0)[j], lpf._ir[j]);
		} else {
			BOOST_CHECK_EQUAL (out->data(0)[j], 0);
		}
	}

	HighPassAudioFilter hpf (0.02, 0.3);

	in = make_shared<AudioBuffers>(1, 9133);
	in->make_silent ();
	in->data(0)[0] = 1;

	out = hpf.run (in);
	for (int j = 0; j < out->frames(); ++j) {
		if (j <= hpf._M) {
			BOOST_CHECK_EQUAL (out->data(0)[j], hpf._ir[j]);
		} else {
			BOOST_CHECK_EQUAL (out->data(0)[j], 0);
		}
	}
}

/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/audio_merger.h"
#include "lib/audio_buffers.h"
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/signals2.hpp>

using std::pair;
using boost::shared_ptr;
using boost::bind;

static shared_ptr<const AudioBuffers> last_audio;

int const sampling_rate = 48000;

BOOST_AUTO_TEST_CASE (audio_merger_test1)
{
	AudioMerger merger (1, sampling_rate);

	/* Push 64 samples, 0 -> 63 at time 0 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, DCPTime());

	/* Push 64 samples, 0 -> 63 at time 22 */
	merger.push (buffers, DCPTime::from_frames (22, sampling_rate));

	pair<shared_ptr<AudioBuffers>, DCPTime> tb = merger.pull (DCPTime::from_frames (22, sampling_rate));
	BOOST_CHECK (tb.first != shared_ptr<const AudioBuffers> ());
	BOOST_CHECK_EQUAL (tb.first->frames(), 22);
	BOOST_CHECK_EQUAL (tb.second.get(), 0);

	/* And they should be a staircase */
	for (int i = 0; i < 22; ++i) {
		BOOST_CHECK_EQUAL (tb.first->data()[0][i], i);
	}

	tb = merger.pull (DCPTime::from_frames (22 + 64, sampling_rate));

	BOOST_CHECK_EQUAL (tb.first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.second.get(), DCPTime::from_frames(22, sampling_rate).get());

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		int correct = i;
		if (i < (64 - 22)) {
			correct += i + 22;
		}
		BOOST_CHECK_EQUAL (tb.first->data()[0][i], correct);
	}
}

BOOST_AUTO_TEST_CASE (audio_merger_test2)
{
	AudioMerger merger (1, sampling_rate);

	/* Push 64 samples, 0 -> 63 at time 9 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, DCPTime::from_frames (9, sampling_rate));

	pair<shared_ptr<AudioBuffers>, DCPTime> tb = merger.pull (DCPTime::from_frames (9, sampling_rate));
	BOOST_CHECK_EQUAL (tb.first->frames(), 9);
	BOOST_CHECK_EQUAL (tb.second.get(), 0);

	for (int i = 0; i < 9; ++i) {
		BOOST_CHECK_EQUAL (tb.first->data()[0][i], 0);
	}

	tb = merger.pull (DCPTime::from_frames (9 + 64, sampling_rate));

	BOOST_CHECK_EQUAL (tb.first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.second.get(), DCPTime::from_frames(9, sampling_rate).get());

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (tb.first->data()[0][i], i);
	}
}

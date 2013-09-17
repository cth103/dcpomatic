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
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/signals2.hpp>
#include "lib/audio_merger.h"
#include "lib/audio_buffers.h"

using boost::shared_ptr;
using boost::bind;

static shared_ptr<const AudioBuffers> last_audio;

static int
pass_through (int x)
{
	return x;
}

BOOST_AUTO_TEST_CASE (audio_merger_test1)
{
	AudioMerger<int, int> merger (1, bind (&pass_through, _1), boost::bind (&pass_through, _1));

	/* Push 64 samples, 0 -> 63 at time 0 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, 0);

	/* Push 64 samples, 0 -> 63 at time 22 */
	merger.push (buffers, 22);

	TimedAudioBuffers<int> tb = merger.pull (22);
	BOOST_CHECK (tb.audio != shared_ptr<const AudioBuffers> ());
	BOOST_CHECK_EQUAL (tb.audio->frames(), 22);
	BOOST_CHECK_EQUAL (tb.time, 0);

	/* And they should be a staircase */
	for (int i = 0; i < 22; ++i) {
		BOOST_CHECK_EQUAL (tb.audio->data()[0][i], i);
	}

	tb = merger.flush ();

	/* That flush should give us 64 samples at 22 */
	BOOST_CHECK_EQUAL (tb.audio->frames(), 64);
	BOOST_CHECK_EQUAL (tb.time, 22);

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		int correct = i;
		if (i < (64 - 22)) {
			correct += i + 22;
		}
		BOOST_CHECK_EQUAL (tb.audio->data()[0][i], correct);
	}
}

BOOST_AUTO_TEST_CASE (audio_merger_test2)
{
	AudioMerger<int, int> merger (1, bind (&pass_through, _1), boost::bind (&pass_through, _1));

	/* Push 64 samples, 0 -> 63 at time 9 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, 9);

	TimedAudioBuffers<int> tb = merger.pull (9);
	BOOST_CHECK_EQUAL (tb.audio->frames(), 9);
	BOOST_CHECK_EQUAL (tb.time, 0);
	
	for (int i = 0; i < 9; ++i) {
		BOOST_CHECK_EQUAL (tb.audio->data()[0][i], 0);
	}
	
	tb = merger.flush ();

	/* That flush should give us 64 samples at 9 */
	BOOST_CHECK_EQUAL (tb.audio->frames(), 64);
	BOOST_CHECK_EQUAL (tb.time, 9);
	
	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (tb.audio->data()[0][i], i);
	}
}

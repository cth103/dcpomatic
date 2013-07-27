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
static int last_time;

static int
pass_through (int x)
{
	return x;
}

static void
process_audio (shared_ptr<const AudioBuffers> audio, int time)
{
	last_audio = audio;
	last_time = time;
}

static void
reset ()
{
	last_audio.reset ();
	last_time = 0;
}

BOOST_AUTO_TEST_CASE (audio_merger_test1)
{
	AudioMerger<int, int> merger (1, bind (&pass_through, _1), boost::bind (&pass_through, _1));
	merger.Audio.connect (bind (&process_audio, _1, _2));

	reset ();
	
	/* Push 64 samples, 0 -> 63 at time 0 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, 0);

	/* That should not have caused an emission */
	BOOST_CHECK_EQUAL (last_audio, shared_ptr<const AudioBuffers> ());
	BOOST_CHECK_EQUAL (last_time, 0);

	/* Push 64 samples, 0 -> 63 at time 22 */
	merger.push (buffers, 22);

	/* That should have caused an emission of 22 samples at 0 */
	BOOST_CHECK (last_audio != shared_ptr<const AudioBuffers> ());
	BOOST_CHECK_EQUAL (last_audio->frames(), 22);
	BOOST_CHECK_EQUAL (last_time, 0);

	/* And they should be a staircase */
	for (int i = 0; i < 22; ++i) {
		BOOST_CHECK_EQUAL (last_audio->data()[0][i], i);
	}

	reset ();
	merger.flush ();

	/* That flush should give us 64 samples at 22 */
	BOOST_CHECK_EQUAL (last_audio->frames(), 64);
	BOOST_CHECK_EQUAL (last_time, 22);

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		int correct = i;
		if (i < (64 - 22)) {
			correct += i + 22;
		}
		BOOST_CHECK_EQUAL (last_audio->data()[0][i], correct);
	}
}

BOOST_AUTO_TEST_CASE (audio_merger_test2)
{
	AudioMerger<int, int> merger (1, bind (&pass_through, _1), boost::bind (&pass_through, _1));
	merger.Audio.connect (bind (&process_audio, _1, _2));

	reset ();
	
	/* Push 64 samples, 0 -> 63 at time 9 */
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, 64));
	for (int i = 0; i < 64; ++i) {
		buffers->data()[0][i] = i;
	}
	merger.push (buffers, 9);

	/* That flush should give us 9 samples at 0 */
	BOOST_CHECK_EQUAL (last_audio->frames(), 9);
	BOOST_CHECK_EQUAL (last_time, 0);
	
	for (int i = 0; i < 9; ++i) {
		BOOST_CHECK_EQUAL (last_audio->data()[0][i], 0);
	}
	
	reset ();
	merger.flush ();

	/* That flush should give us 64 samples at 9 */
	BOOST_CHECK_EQUAL (last_audio->frames(), 64);
	BOOST_CHECK_EQUAL (last_time, 9);
	
	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (last_audio->data()[0][i], i);
	}
}

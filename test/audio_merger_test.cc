/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_merger_test.cc
 *  @brief Test AudioMerger class.
 *  @ingroup selfcontained
 */

#include "lib/cross.h"
#include "lib/audio_merger.h"
#include "lib/audio_buffers.h"
#include "lib/dcpomatic_time.h"
#include "test.h"
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include <boost/signals2.hpp>
#include <iostream>

using std::pair;
using std::list;
using std::cout;
using std::string;
using std::shared_ptr;
using boost::bind;
using namespace dcpomatic;

static shared_ptr<const AudioBuffers> last_audio;

int const sampling_rate = 48000;

static void
push (AudioMerger& merger, int from, int to, int at)
{
	shared_ptr<AudioBuffers> buffers (new AudioBuffers (1, to - from));
	for (int i = 0; i < (to - from); ++i) {
		buffers->data()[0][i] = from + i;
	}
	merger.push (buffers, DCPTime(at, sampling_rate));
}

/* Basic mixing, 2 overlapping pushes */
BOOST_AUTO_TEST_CASE (audio_merger_test1)
{
	AudioMerger merger (sampling_rate);

	push (merger, 0, 64, 0);
	push (merger, 0, 64, 22);

	list<pair<shared_ptr<AudioBuffers>, DCPTime> > tb = merger.pull (DCPTime::from_frames (22, sampling_rate));
	BOOST_REQUIRE (tb.size() == 1U);
	BOOST_CHECK (tb.front().first != shared_ptr<const AudioBuffers> ());
	BOOST_CHECK_EQUAL (tb.front().first->frames(), 22);
	BOOST_CHECK_EQUAL (tb.front().second.get(), 0);

	/* And they should be a staircase */
	for (int i = 0; i < 22; ++i) {
		BOOST_CHECK_EQUAL (tb.front().first->data()[0][i], i);
	}

	tb = merger.pull (DCPTime::from_frames (22 + 64, sampling_rate));
	BOOST_REQUIRE (tb.size() == 1U);
	BOOST_CHECK_EQUAL (tb.front().first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.front().second.get(), DCPTime::from_frames(22, sampling_rate).get());

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		int correct = i;
		if (i < (64 - 22)) {
			correct += i + 22;
		}
		BOOST_CHECK_EQUAL (tb.front().first->data()[0][i], correct);
	}
}

/* Push at non-zero time */
BOOST_AUTO_TEST_CASE (audio_merger_test2)
{
	AudioMerger merger (sampling_rate);

	push (merger, 0, 64, 9);

	/* There's nothing from 0 to 9 */
	list<pair<shared_ptr<AudioBuffers>, DCPTime> > tb = merger.pull (DCPTime::from_frames (9, sampling_rate));
	BOOST_CHECK_EQUAL (tb.size(), 0U);

	/* Then there's our data at 9 */
	tb = merger.pull (DCPTime::from_frames (9 + 64, sampling_rate));

	BOOST_CHECK_EQUAL (tb.front().first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.front().second.get(), DCPTime::from_frames(9, sampling_rate).get());

	/* Check the sample values */
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (tb.front().first->data()[0][i], i);
	}
}

/* Push two non contiguous blocks */
BOOST_AUTO_TEST_CASE (audio_merger_test3)
{
	AudioMerger merger (sampling_rate);

	push (merger, 0, 64, 17);
	push (merger, 0, 64, 114);

	/* Get them back */

	list<pair<shared_ptr<AudioBuffers>, DCPTime> > tb = merger.pull (DCPTime::from_frames (100, sampling_rate));
	BOOST_REQUIRE (tb.size() == 1U);
	BOOST_CHECK_EQUAL (tb.front().first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.front().second.get(), DCPTime::from_frames(17, sampling_rate).get());
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (tb.front().first->data()[0][i], i);
	}

	tb = merger.pull (DCPTime::from_frames (200, sampling_rate));
	BOOST_REQUIRE (tb.size() == 1U);
	BOOST_CHECK_EQUAL (tb.front().first->frames(), 64);
	BOOST_CHECK_EQUAL (tb.front().second.get(), DCPTime::from_frames(114, sampling_rate).get());
	for (int i = 0; i < 64; ++i) {
		BOOST_CHECK_EQUAL (tb.front().first->data()[0][i], i);
	}
}

/* Reply a sequence of calls to AudioMerger that resulted in a crash */
BOOST_AUTO_TEST_CASE (audio_merger_test4)
{
	FILE* f = fopen_boost("test/data/audio_merger_bug1.log", "r");
	BOOST_REQUIRE (f);
	list<string> tokens;
	char buf[64];
	while (fscanf(f, "%63s", buf) == 1) {
		tokens.push_back (buf);
	}

	shared_ptr<AudioMerger> merger;
	list<string>::const_iterator i = tokens.begin ();
	while (i != tokens.end()) {
		BOOST_CHECK (*i++ == "I/AM");
		string const cmd = *i++;
		if (cmd == "frame_rate") {
			BOOST_REQUIRE (i != tokens.end());
			merger.reset (new AudioMerger(dcp::raw_convert<int>(*i++)));
		} else if (cmd == "clear") {
			merger->clear ();
		} else if (cmd == "push") {
			BOOST_REQUIRE (i != tokens.end());
			DCPTime time(dcp::raw_convert<DCPTime::Type>(*i++));
			BOOST_REQUIRE (i != tokens.end());
			int const frames = dcp::raw_convert<int>(*i++);
			shared_ptr<AudioBuffers> buffers(new AudioBuffers(1, frames));
			BOOST_REQUIRE (merger);
			merger->push (buffers, time);
		} else if (cmd == "pull") {
			BOOST_REQUIRE (i != tokens.end());
			DCPTime time(dcp::raw_convert<DCPTime::Type>(*i++));
			merger->pull (time);
		}
	}
}



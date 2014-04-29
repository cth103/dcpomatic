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

/** @file  test/audio_buffers_test.cc
 *  @brief Test AudioBuffers in various ways.
 */

#include <cmath>
#include <boost/test/unit_test.hpp>
#include "lib/audio_buffers.h"

using std::pow;

static float tolerance = 1e-3;

static float
random_float ()
{
	return float (rand ()) / RAND_MAX;
}

static void
random_fill (AudioBuffers& buffers)
{
	for (int i = 0; i < buffers.frames(); ++i) {
		for (int j = 0; j < buffers.channels(); ++j) {
			buffers.data(j)[i] = random_float ();
		}
	}
}

static void
random_check (AudioBuffers& buffers, int from, int frames)
{
	for (int i = from; i < (from + frames); ++i) {
		for (int j = 0; j < buffers.channels(); ++j) {
			BOOST_CHECK_CLOSE (buffers.data(j)[i], random_float (), tolerance);
		}
	}
}

/** Basic setup */
BOOST_AUTO_TEST_CASE (audio_buffers_setup_test)
{
	AudioBuffers buffers (4, 9155);

	BOOST_CHECK (buffers.data ());
	for (int i = 0; i < 4; ++i) {
		BOOST_CHECK (buffers.data (i));
	}

	BOOST_CHECK_EQUAL (buffers.channels(), 4);
	BOOST_CHECK_EQUAL (buffers.frames(), 9155);
}

/** Extending some buffers */
BOOST_AUTO_TEST_CASE (audio_buffers_extend_test)
{
	AudioBuffers buffers (3, 150);
	srand (1);
	random_fill (buffers);

	/* Extend */
	buffers.ensure_size (299);

	srand (1);
	random_check (buffers, 0, 150);

	/* New space should be silent */
	for (int i = 150; i < 299; ++i) {
		for (int c = 0; c < 3; ++c) {
			BOOST_CHECK_EQUAL (buffers.data(c)[i], 0);
		}
	}
}

/** make_silent() */
BOOST_AUTO_TEST_CASE (audio_buffers_make_silent_test)
{
	AudioBuffers buffers (9, 9933);
	srand (2);
	random_fill (buffers);

	buffers.make_silent ();
	
	for (int i = 0; i < 9933; ++i) {
		for (int c = 0; c < 9; ++c) {
			BOOST_CHECK_EQUAL (buffers.data(c)[i], 0);
		}
	}
}

/** make_silent (int c) */
BOOST_AUTO_TEST_CASE (audio_buffers_make_silent_channel_test)
{
	AudioBuffers buffers (9, 9933);
	srand (3);
	random_fill (buffers);

	buffers.make_silent (4);

	srand (3);
	for (int i = 0; i < 9933; ++i) {
		for (int c = 0; c < 9; ++c) {
			if (c == 4) {
				random_float ();
				BOOST_CHECK_EQUAL (buffers.data(c)[i], 0);
			} else {
				BOOST_CHECK_CLOSE (buffers.data(c)[i], random_float (), tolerance);
			}
		}
	}
}

/** make_silent (int from, int frames) */
BOOST_AUTO_TEST_CASE (audio_buffers_make_silent_part_test)
{
	AudioBuffers buffers (9, 9933);
	srand (4);
	random_fill (buffers);

	buffers.make_silent (145, 833);

	srand (4);
	for (int i = 0; i < 145; ++i) {
		for (int c = 0; c < 9; ++c) {
			BOOST_CHECK_EQUAL (buffers.data(c)[i], random_float ());
		}
	}

	for (int i = 145; i < (145 + 833); ++i) {
		for (int c = 0; c < 9; ++c) {
			random_float ();
			BOOST_CHECK_EQUAL (buffers.data(c)[i], 0);
		}
	}

	for (int i = (145 + 833); i < 9933; ++i) {
		for (int c = 0; c < 9; ++c) {
			BOOST_CHECK_EQUAL (buffers.data(c)[i], random_float ());
		}
	}
}

/* apply_gain */
BOOST_AUTO_TEST_CASE (audio_buffers_apply_gain)
{
	AudioBuffers buffers (2, 417315);
	srand (9);
	random_fill (buffers);

	buffers.apply_gain (5.4);

	srand (9);
	for (int i = 0; i < 417315; ++i) {
		for (int c = 0; c < 2; ++c) {
			BOOST_CHECK_CLOSE (buffers.data(c)[i], random_float() * pow (10, 5.4 / 20), tolerance);
		}
	}
}

/* copy_from */
BOOST_AUTO_TEST_CASE (audio_buffers_copy_from)
{
	AudioBuffers a (5, 63711);
	AudioBuffers b (5, 12345);

	srand (42);
	random_fill (a);

	srand (99);
	random_fill (b);

	a.copy_from (&b, 517, 233, 194);

	/* Re-seed a's generator and check the numbers that came from it */

	/* First part; not copied-over */
	srand (42);
	random_check (a, 0, 194);

	/* Second part; copied-over (just burn generator a's numbers) */
	for (int i = 0; i < (517 * 5); ++i) {
		random_float ();
	}

	/* Third part; not copied-over */
	random_check (a, 194 + 517, a.frames() - 194 - 517);

	/* Re-seed b's generator and check the numbers that came from it */
	srand (99);

	/* First part; burn */
	for (int i = 0; i < 194 * 5; ++i) {
		random_float ();
	}

	/* Second part; copied */
	random_check (b, 194, 517);
}

/* move */
BOOST_AUTO_TEST_CASE (audio_buffers_move)
{
	AudioBuffers buffers (7, 65536);

	srand (84);
	random_fill (buffers);

	int const from = 888;
	int const to = 666;
	int const frames = 444;

	buffers.move (from, to, frames);

	/* Re-seed and check the un-moved parts */
	srand (84);

	random_check (buffers, 0, to);

	/* Burn a few */
	for (int i = 0; i < (from - to + frames) * 7; ++i) {
		random_float ();
	}

	random_check (buffers, from + frames, 65536 - frames - from);

	/* Re-seed and check the moved part */
	srand (84);

	/* Burn a few */
	for (int i = 0; i < from * 7; ++i) {
		random_float ();
	}
	
	random_check (buffers, to, frames);
}

/** accumulate_channel */
BOOST_AUTO_TEST_CASE (audio_buffers_accumulate_channel)
{
	AudioBuffers a (3, 256);
	srand (38);
	random_fill (a);
	
	AudioBuffers b (3, 256);
	random_fill (b);

	a.accumulate_channel (&b, 2, 1, 1.2);

	srand (38);
	for (int i = 0; i < 256; ++i) {
		for (int c = 0; c < 3; ++c) {
			float const A = random_float ();
			if (c == 1) {
				BOOST_CHECK_CLOSE (a.data(c)[i], A + b.data(2)[i] * 1.2, tolerance);
			} else {
				BOOST_CHECK_CLOSE (a.data(c)[i], A, tolerance);
			}
		}
	}
}

/** accumulate_frames */
BOOST_AUTO_TEST_CASE (audio_buffers_accumulate_frames)
{
	AudioBuffers a (3, 256);
	srand (38);
	random_fill (a);
	
	AudioBuffers b (3, 256);
	random_fill (b);

	a.accumulate_frames (&b, 91, 44, 129);

	srand (38);
	for (int i = 0; i < 256; ++i) {
		for (int c = 0; c < 3; ++c) {
			float const A = random_float ();
			if (i < 44 || i >= (44 + 129)) {
				BOOST_CHECK_CLOSE (a.data(c)[i], A, tolerance);
			} else {
				BOOST_CHECK_CLOSE (a.data(c)[i], A + b.data(c)[i + 91 - 44], tolerance);
			}
		}
	}
}

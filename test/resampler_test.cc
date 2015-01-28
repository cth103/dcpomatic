/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/resampler_test.cc
 *  @brief Check that the timings that come back from the resampler correspond
 *  to the number of samples it generates.
 */

#include <boost/test/unit_test.hpp>
#include "lib/audio_buffers.h"
#include "lib/resampler.h"

using std::pair;
using std::cout;
using boost::shared_ptr;

static void
resampler_test_one (int from, int to)
{
	Resampler resamp (from, to, 1);

	/* 3 hours */
	int64_t const N = int64_t (from) * 60 * 60 * 3;
		
	/* XXX: no longer checks anything */
	for (int64_t i = 0; i < N; i += 1000) {
		shared_ptr<AudioBuffers> a (new AudioBuffers (1, 1000));
		a->make_silent ();
		shared_ptr<const AudioBuffers> r = resamp.run (a);
	}
}	
		
BOOST_AUTO_TEST_CASE (resampler_test)
{
	resampler_test_one (44100, 48000);
	resampler_test_one (44100, 46080);
	resampler_test_one (44100, 50000);
}

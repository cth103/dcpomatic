/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
#include "lib/audio_analysis.h"

static float
random_float ()
{
	return (float (rand ()) / RAND_MAX) * 2 - 1;
}

/* Check serialisation of audio analyses */
BOOST_AUTO_TEST_CASE (audio_analysis_test)
{
	int const channels = 3;
	int const points = 4096;
	
	srand (1);
	
	AudioAnalysis a (3);
	for (int i = 0; i < channels; ++i) {
		for (int j = 0; j < points; ++j) {
			AudioPoint p;
			p[AudioPoint::PEAK] = random_float ();
			p[AudioPoint::RMS] = random_float ();
			a.add_point (i, p);
		}
	}

	a.write ("build/test/audio_analysis_test");

	srand (1);

	AudioAnalysis b ("build/test/audio_analysis_test");
	for (int i = 0; i < channels; ++i) {
		BOOST_CHECK (b.points(i) == points);
		for (int j = 0; j < points; ++j) {
			AudioPoint p = b.get_point (i, j);
			BOOST_CHECK_CLOSE (p[AudioPoint::PEAK], random_float (), 0.1);
			BOOST_CHECK_CLOSE (p[AudioPoint::RMS],  random_float (), 0.1);
		}
	}
}

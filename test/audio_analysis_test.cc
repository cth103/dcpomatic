/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_analysis_test.cc
 *  @brief Check audio analysis code.
 */

#include <boost/test/unit_test.hpp>
#include "lib/audio_analysis.h"
#include "lib/film.h"
#include "lib/sndfile_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "test.h"

using boost::shared_ptr;

static float
random_float ()
{
	return (float (rand ()) / RAND_MAX) * 2 - 1;
}

BOOST_AUTO_TEST_CASE (audio_analysis_serialisation_test)
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

	float const peak = random_float ();
	DCPTime const peak_time = DCPTime (rand ());
	a.set_peak (peak, peak_time);

	a.write ("build/test/audio_analysis_serialisation_test");

	srand (1);

	AudioAnalysis b ("build/test/audio_analysis_serialisation_test");
	for (int i = 0; i < channels; ++i) {
		BOOST_CHECK_EQUAL (b.points(i), points);
		for (int j = 0; j < points; ++j) {
			AudioPoint p = b.get_point (i, j);
			BOOST_CHECK_CLOSE (p[AudioPoint::PEAK], random_float (), 1);
			BOOST_CHECK_CLOSE (p[AudioPoint::RMS],  random_float (), 1);
		}
	}
	
	BOOST_CHECK (b.peak ());
	BOOST_CHECK_CLOSE (b.peak().get(), peak, 1);
	BOOST_CHECK (b.peak_time ());
	BOOST_CHECK_EQUAL (b.peak_time().get(), peak_time);
}

static void
finished ()
{

}

BOOST_AUTO_TEST_CASE (audio_analysis_test)
{
	shared_ptr<Film> film = new_test_film ("audio_analysis_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("audio_analysis_test");
	boost::filesystem::path p = private_data / "betty_L.wav";

	shared_ptr<SndfileContent> c (new SndfileContent (film, p));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	c->analyse_audio (boost::bind (&finished));
	wait_for_jobs ();
}

/* Check that audio analysis works (i.e. runs without error) with a -ve delay */
BOOST_AUTO_TEST_CASE (audio_analysis_negative_delay_test)
{
	shared_ptr<Film> film = new_test_film ("audio_analysis_negative_delay_test");
	film->set_name ("audio_analysis_negative_delay_test");
	shared_ptr<AudioContent> c (new FFmpegContent (film, private_data / "boon_telly.mkv"));
	c->set_audio_delay (-250);
	film->examine_and_add_content (c);
	wait_for_jobs ();
	
	c->analyse_audio (boost::bind (&finished));
	wait_for_jobs ();
}

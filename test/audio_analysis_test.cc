/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_analysis_test.cc
 *  @brief Check audio analysis code.
 */

#include <boost/test/unit_test.hpp>
#include "lib/audio_analysis.h"
#include "lib/analyse_audio_job.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/job_manager.h"
#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/playlist.h"
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
	a.set_sample_peak (peak, peak_time);

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

	BOOST_CHECK (b.sample_peak ());
	BOOST_CHECK_CLOSE (b.sample_peak().get(), peak, 1);
	BOOST_CHECK (b.sample_peak_time ());
	BOOST_CHECK_EQUAL (b.sample_peak_time().get(), peak_time);
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

	shared_ptr<FFmpegContent> c (new FFmpegContent (film, p));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	shared_ptr<AnalyseAudioJob> job (new AnalyseAudioJob (film, film->playlist ()));
	job->Finished.connect (boost::bind (&finished));
	JobManager::instance()->add (job);
	wait_for_jobs ();
}

/** Check that audio analysis works (i.e. runs without error) with a -ve delay */
BOOST_AUTO_TEST_CASE (audio_analysis_negative_delay_test)
{
	shared_ptr<Film> film = new_test_film ("audio_analysis_negative_delay_test");
	film->set_name ("audio_analysis_negative_delay_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, private_data / "boon_telly.mkv"));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	c->audio->set_delay (-250);

	shared_ptr<AnalyseAudioJob> job (new AnalyseAudioJob (film, film->playlist ()));
	job->Finished.connect (boost::bind (&finished));
	JobManager::instance()->add (job);
	wait_for_jobs ();
}

/** Check audio analysis that is incorrect in 2e98263 */
BOOST_AUTO_TEST_CASE (audio_analysis_test2)
{
	shared_ptr<Film> film = new_test_film ("audio_analysis_test2");
	film->set_name ("audio_analysis_test2");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, private_data / "3d_thx_broadway_2010_lossless.m2ts"));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	shared_ptr<AnalyseAudioJob> job (new AnalyseAudioJob (film, film->playlist ()));
	job->Finished.connect (boost::bind (&finished));
	JobManager::instance()->add (job);
	wait_for_jobs ();
}


static bool done = false;

static void
analysis_finished ()
{
	done = true;
}

/* Test a case which was reported to throw an exception; analysing
 * a 12-channel DCP's audio.
 */
BOOST_AUTO_TEST_CASE (audio_analysis_test3)
{
	shared_ptr<Film> film = new_test_film ("analyse_audio_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");

	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/white.wav"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	film->set_audio_channels (12);
	boost::signals2::connection connection;
	JobManager::instance()->analyse_audio (film, film->playlist(), connection, boost::bind (&analysis_finished));
	wait_for_jobs ();
	BOOST_CHECK (done);
}

/** Run an audio analysis that triggered an exception in the audio decoder at one point */
BOOST_AUTO_TEST_CASE (analyse_audio_test4)
{
	shared_ptr<Film> film = new_test_film ("analyse_audio_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<Content> content = content_factory (film, private_data / "20 The Wedding Convoy Song.m4a");
	film->examine_and_add_content (content);
	wait_for_jobs ();

	shared_ptr<Playlist> playlist (new Playlist);
	playlist->add (content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio (film, playlist, c, boost::bind (&finished));
	BOOST_CHECK (!wait_for_jobs ());
}

/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @defgroup selfcontained Self-contained tests of single classes / method sets */

/** @file  test/audio_analysis_test.cc
 *  @brief Test AudioAnalysis class.
 *  @ingroup selfcontained
 */


#include "lib/analyse_audio_job.h"
#include "lib/audio_analysis.h"
#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/playlist.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::vector;
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (audio_analysis_serialisation_test)
{
	int const channels = 3;
	int const points = 4096;

	auto random_float = []() {
		return (float (rand ()) / RAND_MAX) * 2 - 1;
	};

	AudioAnalysis a (3);
	for (int i = 0; i < channels; ++i) {
		for (int j = 0; j < points; ++j) {
			AudioPoint p;
			p[AudioPoint::PEAK] = random_float ();
			p[AudioPoint::RMS] = random_float ();
			a.add_point (i, p);
		}
	}

	vector<AudioAnalysis::PeakTime> peak;
	for (int i = 0; i < channels; ++i) {
		peak.push_back (AudioAnalysis::PeakTime(random_float(), DCPTime(rand())));
	}
	a.set_sample_peak (peak);

	a.set_samples_per_point (100);
	a.set_sample_rate (48000);
	a.write ("build/test/audio_analysis_serialisation_test");

	AudioAnalysis b ("build/test/audio_analysis_serialisation_test");
	for (int i = 0; i < channels; ++i) {
		BOOST_CHECK_EQUAL (b.points(i), points);
		for (int j = 0; j < points; ++j) {
			AudioPoint p = a.get_point (i, j);
			AudioPoint q = b.get_point (i, j);
			BOOST_CHECK_CLOSE (p[AudioPoint::PEAK], q[AudioPoint::PEAK], 1);
			BOOST_CHECK_CLOSE (p[AudioPoint::RMS],  q[AudioPoint::RMS], 1);
		}
	}

	BOOST_REQUIRE_EQUAL (b.sample_peak().size(), 3U);
	for (int i = 0; i < channels; ++i) {
		BOOST_CHECK_CLOSE (b.sample_peak()[i].peak, peak[i].peak, 1);
		BOOST_CHECK_EQUAL (b.sample_peak()[i].time.get(), peak[i].time.get());
	}

	BOOST_CHECK_EQUAL (a.samples_per_point(), 100);
	BOOST_CHECK_EQUAL (a.sample_rate(), 48000);
}


BOOST_AUTO_TEST_CASE (audio_analysis_test)
{
	auto film = new_test_film ("audio_analysis_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("FTR"));
	film->set_container (Ratio::from_id("185"));
	film->set_name ("audio_analysis_test");
	boost::filesystem::path p = TestPaths::private_data() / "betty_L.wav";

	auto c = make_shared<FFmpegContent>(p);
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
}


/** Check that audio analysis works (i.e. runs without error) with a -ve delay */
BOOST_AUTO_TEST_CASE (audio_analysis_negative_delay_test)
{
	auto film = new_test_film ("audio_analysis_negative_delay_test");
	film->set_name ("audio_analysis_negative_delay_test");
	auto c = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->audio->set_delay (-250);

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
}


/** Check audio analysis that is incorrect in 2e98263 */
BOOST_AUTO_TEST_CASE (audio_analysis_test2)
{
	auto film = new_test_film ("audio_analysis_test2");
	film->set_name ("audio_analysis_test2");
	auto c = make_shared<FFmpegContent>(TestPaths::private_data() / "3d_thx_broadway_2010_lossless.m2ts");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
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
	auto film = new_test_film ("analyse_audio_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("TLR"));
	film->set_name ("frobozz");

	auto content = make_shared<FFmpegContent>("test/data/white.wav");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_audio_channels (12);
	boost::signals2::connection connection;
	JobManager::instance()->analyse_audio(film, film->playlist(), false, connection, boost::bind(&analysis_finished));
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK (done);
}


/** Run an audio analysis that triggered an exception in the audio decoder at one point */
BOOST_AUTO_TEST_CASE (analyse_audio_test4)
{
	auto film = new_test_film ("analyse_audio_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("TLR"));
	film->set_name ("frobozz");
	auto content = content_factory(TestPaths::private_data() / "20 The Wedding Convoy Song.m4a")[0];
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	auto playlist = make_shared<Playlist>();
	playlist->add (film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, []() {});
	BOOST_CHECK (!wait_for_jobs ());
}


BOOST_AUTO_TEST_CASE (analyse_audio_leqm_test)
{
	auto film = new_test_film2 ("analyse_audio_leqm_test");
	film->set_audio_channels (2);
	auto content = content_factory(TestPaths::private_data() / "betty_stereo_48k.wav")[0];
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	auto playlist = make_shared<Playlist>();
	playlist->add (film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, []() {});
	BOOST_CHECK (!wait_for_jobs());

	AudioAnalysis analysis(film->audio_analysis_path(playlist));

	/* The CLI tool of leqm_nrt gives this value for betty_stereo_48k.wav */
	BOOST_CHECK_CLOSE (analysis.leqm().get_value_or(0), 88.276, 0.001);
}


/** Bug #2364; a file with a lot of silent video at the end (about 50s worth)
 *  crashed the audio analysis with an OOM on Windows.
 */
BOOST_AUTO_TEST_CASE(analyse_audio_with_long_silent_end)
{
	auto content = content_factory(TestPaths::private_data() / "2364.mkv")[0];
	auto film = new_test_film2("analyse_audio_with_long_silent_end", { content });

	auto playlist = make_shared<Playlist>();
	playlist->add(film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, []() {});
	BOOST_CHECK(!wait_for_jobs());
}

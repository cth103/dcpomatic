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
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/playlist.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <numeric>


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
	auto c = make_shared<FFmpegContent>(TestPaths::private_data() / "betty_L.wav");
	auto film = new_test_film("audio_analysis_test", { c });

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
}


/** Check that audio analysis works (i.e. runs without error) with a -ve delay */
BOOST_AUTO_TEST_CASE (audio_analysis_negative_delay_test)
{
	auto c = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	auto film = new_test_film("audio_analysis_negative_delay_test", { c });
	c->audio->set_delay (-250);

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
}


/** Check audio analysis that is incorrect in 2e98263 */
BOOST_AUTO_TEST_CASE (audio_analysis_test2)
{
	auto c = make_shared<FFmpegContent>(TestPaths::private_data() / "3d_thx_broadway_2010_lossless.m2ts");
	auto film = new_test_film("audio_analysis_test2", { c });

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());
}


/* Test a case which was reported to throw an exception; analysing
 * a 12-channel DCP's audio.
 */
BOOST_AUTO_TEST_CASE (audio_analysis_test3)
{
	auto content = make_shared<FFmpegContent>("test/data/white.wav");
	auto film = new_test_film("analyse_audio_test", { content });
	film->set_audio_channels (12);

	boost::signals2::connection connection;
	bool done = false;
	JobManager::instance()->analyse_audio(film, film->playlist(), false, connection, [&done](Job::Result) { done = true; });
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK (done);
}


/** Run an audio analysis that triggered an exception in the audio decoder at one point */
BOOST_AUTO_TEST_CASE (analyse_audio_test4)
{
	auto content = content_factory(TestPaths::private_data() / "20 The Wedding Convoy Song.m4a")[0];
	auto film = new_test_film("analyse_audio_test", { content });

	auto playlist = make_shared<Playlist>();
	playlist->add (film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
	BOOST_CHECK (!wait_for_jobs ());
}


BOOST_AUTO_TEST_CASE (analyse_audio_leqm_test)
{
	auto film = new_test_film("analyse_audio_leqm_test");
	film->set_audio_channels (2);
	auto content = content_factory(TestPaths::private_data() / "betty_stereo_48k.wav")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	auto playlist = make_shared<Playlist>();
	playlist->add (film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
	BOOST_CHECK (!wait_for_jobs());

	AudioAnalysis analysis(film->audio_analysis_path(playlist));

	/* The CLI tool of leqm_nrt gives this value for betty_stereo_48k.wav */
	BOOST_CHECK_CLOSE (analysis.leqm().get_value_or(0), 88.276, 0.001);
}


BOOST_AUTO_TEST_CASE(analyse_audio_leqm_same_with_empty_channels)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film("analyse_audio_leqm_test2", { dcp });
	film->set_audio_channels(8);

	auto analyse = [film, dcp](int channels) {
		film->set_audio_channels(channels);
		auto playlist = make_shared<Playlist>();
		playlist->add(film, dcp);
		boost::signals2::connection c;
		JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
		BOOST_CHECK(!wait_for_jobs());
		AudioAnalysis analysis(film->audio_analysis_path(playlist));
		return analysis.leqm().get_value_or(0);
	};

	BOOST_CHECK_CLOSE(analyse( 6), 84.51411, 0.001);
	BOOST_CHECK_CLOSE(analyse( 8), 84.51411, 0.001);
	BOOST_CHECK_CLOSE(analyse(16), 84.51411, 0.001);
}


/** Bug #2364; a file with a lot of silent video at the end (about 50s worth)
 *  crashed the audio analysis with an OOM on Windows.
 */
BOOST_AUTO_TEST_CASE(analyse_audio_with_long_silent_end)
{
	auto content = content_factory(TestPaths::private_data() / "2364.mkv")[0];
	auto film = new_test_film("analyse_audio_with_long_silent_end", { content });

	auto playlist = make_shared<Playlist>();
	playlist->add(film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
	BOOST_CHECK(!wait_for_jobs());
}


BOOST_AUTO_TEST_CASE(analyse_audio_with_strange_channel_count)
{
	auto content = content_factory(TestPaths::private_data() / "mali.mkv")[0];
	auto film = new_test_film("analyse_audio_with_strange_channel_count", { content });

	auto playlist = make_shared<Playlist>();
	playlist->add(film, content);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
	BOOST_CHECK(!wait_for_jobs());
}


BOOST_AUTO_TEST_CASE(analyse_audio_with_more_channels_than_film)
{
	auto picture = content_factory("test/data/flat_red.png");
	auto film_16ch = new_test_film("analyse_audio_with_more_channels_than_film_16ch", picture);
	film_16ch->set_audio_channels(16);
	make_and_verify_dcp(film_16ch);

	auto pcm_16ch = find_file(film_16ch->dir(film_16ch->dcp_name()), "pcm_");
	auto sound = content_factory(pcm_16ch)[0];

	auto film_6ch = new_test_film("analyse_audio_with_more_channels_than_film_6ch", { sound });

	auto playlist = make_shared<Playlist>();
	playlist->add(film_6ch, sound);
	boost::signals2::connection c;
	JobManager::instance()->analyse_audio(film_6ch, playlist, false, c, [](Job::Result) {});
	BOOST_CHECK(!wait_for_jobs());
}


BOOST_AUTO_TEST_CASE(analyse_audio_uses_processor_when_analysing_whole_film)
{
	auto sound = content_factory(TestPaths::private_data() / "betty_stereo.wav")[0];
	auto film = new_test_film("analyse_audio_uses_processor_when_analysing_whole_film", { sound });

	auto job = make_shared<AnalyseAudioJob>(film, film->playlist(), true);
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());

	AudioAnalysis analysis(job->path());

	BOOST_REQUIRE(analysis.channels() > 2);
	bool centre_non_zero = false;
	/* Make sure there's something from the mid-side decoder on the centre channel */
	for (auto point = 0; point < analysis.points(2); ++point) {
		if (std::abs(analysis.get_point(2, point)[AudioPoint::Type::PEAK]) > 0) {
			centre_non_zero = true;
		}
	}

	BOOST_CHECK(centre_non_zero);
}


#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
BOOST_AUTO_TEST_CASE(ebur128_test)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film("ebur128_test", { dcp });
	film->set_audio_channels(8);

	auto analyse = [film, dcp](int channels) {
		film->set_audio_channels(channels);
		auto playlist = make_shared<Playlist>();
		playlist->add(film, dcp);
		boost::signals2::connection c;
		JobManager::instance()->analyse_audio(film, playlist, false, c, [](Job::Result) {});
		BOOST_CHECK(!wait_for_jobs());
		return AudioAnalysis(film->audio_analysis_path(playlist));
	};

	auto six = analyse(6);
	BOOST_CHECK_CLOSE(six.true_peak()[0], 0.520668, 1);
	BOOST_CHECK_CLOSE(six.true_peak()[1], 0.519579, 1);
	BOOST_CHECK_CLOSE(six.true_peak()[2], 0.533980, 1);
	BOOST_CHECK_CLOSE(six.true_peak()[3], 0.326270, 1);
	BOOST_CHECK_CLOSE(six.true_peak()[4], 0.363581, 1);
	BOOST_CHECK_CLOSE(six.true_peak()[5], 0.317751, 1);
	BOOST_CHECK_CLOSE(six.overall_true_peak().get(), 0.53398, 1);
	BOOST_CHECK_CLOSE(six.overall_true_peak().get(), 0.53398, 1);
	BOOST_CHECK_CLOSE(six.integrated_loudness().get(), -18.1432, 1);
	BOOST_CHECK_CLOSE(six.loudness_range().get(), 6.92, 1);
}
#endif


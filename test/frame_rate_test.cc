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

/** @file  test/frame_rate_test.cc
 *  @brief Tests for FrameRateChange and the computation of the best
 *  frame rate for the DCP.
 *  @ingroup feature
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/config.h"
#include "lib/ffmpeg_content.h"
#include "lib/playlist.h"
#include "lib/ffmpeg_audio_stream.h"
#include "lib/frame_rate_change.h"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include "test.h"

using std::shared_ptr;

/* Test Playlist::best_dcp_frame_rate and FrameRateChange
   with a single piece of content.
*/
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test_single)
{
	auto film = new_test_film("best_dcp_frame_rate_test_single");
	/* Get any piece of content, it doesn't matter what */
	auto content = std::make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Run some tests with a limited range of allowed rates */

	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 60;
	int best = film->best_video_frame_rate ();
	auto frc = FrameRateChange(60, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 50;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (50, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 48;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (48, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 30;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (30, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 29.97;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (29.97, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 30 / 29.97, 0.1);

	content->_video_frame_rate = 25;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (25, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 24;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (24, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 14.5;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (14.5, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 15 / 14.5, 0.1);

	content->_video_frame_rate = 12.6;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (12.6, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 25 / 25.2, 0.1);

	content->_video_frame_rate = 12.4;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (12.4, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 25 / 24.8, 0.1);

	content->_video_frame_rate = 12;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (12, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	/* Now add some more rates and see if it will use them
	   in preference to skip/repeat.
	*/

	afr.push_back (48);
	afr.push_back (50);
	afr.push_back (60);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 60;
	best = film->playlist()->best_video_frame_rate ();
	frc = FrameRateChange (60, best);
	BOOST_CHECK_EQUAL (best, 60);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 50;
	best = film->playlist()->best_video_frame_rate ();
	frc = FrameRateChange (50, best);
	BOOST_CHECK_EQUAL (best, 50);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	content->_video_frame_rate = 48;
	best = film->playlist()->best_video_frame_rate ();
	frc = FrameRateChange (48, best);
	BOOST_CHECK_EQUAL (best, 48);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	BOOST_CHECK_CLOSE (frc.speed_up, 1, 0.1);

	/* Check some out-there conversions (not the best) */

	frc = FrameRateChange (14.99, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 24 / (2 * 14.99), 0.1);

	/* Check some conversions with limited DCP targets */

	afr.clear ();
	afr.push_back (24);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 25;
	best = film->best_video_frame_rate ();
	frc = FrameRateChange (25, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	BOOST_CHECK_CLOSE (frc.speed_up, 24.0 / 25, 0.1);
}

/* Test Playlist::best_dcp_frame_rate and FrameRateChange
   with two pieces of content.
*/
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test_double)
{
	auto film = new_test_film("best_dcp_frame_rate_test_double");
	/* Get any old content, it doesn't matter what */
	auto A = std::make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (A);
	auto B = std::make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Run some tests with a limited range of allowed rates */

	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	A->_video_frame_rate = 30;
	B->_video_frame_rate = 24;
	BOOST_CHECK_EQUAL (film->best_video_frame_rate(), 25);

	A->_video_frame_rate = 24;
	B->_video_frame_rate = 24;
	BOOST_CHECK_EQUAL (film->best_video_frame_rate(), 24);

	A->_video_frame_rate = 24;
	B->_video_frame_rate = 48;
	BOOST_CHECK_EQUAL (film->best_video_frame_rate(), 24);
}

BOOST_AUTO_TEST_CASE (audio_sampling_rate_test)
{
	auto film = new_test_film("audio_sampling_rate_test");
	/* Get any piece of content, it doesn't matter what */
	auto content = std::make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	shared_ptr<FFmpegAudioStream> stream (new FFmpegAudioStream ("foo", 0, 0, 0, 0));
	content->audio.reset (new AudioContent (content.get()));
	content->audio->add_stream (stream);
	content->_video_frame_rate = 24;
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 48000);

	stream->_frame_rate = 44100;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 48000);

	stream->_frame_rate = 80000;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 48000);

	content->_video_frame_rate = 23.976;
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 47952);

	content->_video_frame_rate = 29.97;
	film->set_video_frame_rate (30);
	BOOST_CHECK_EQUAL (film->video_frame_rate (), 30);
	stream->_frame_rate = 48000;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 47952);

	content->_video_frame_rate = 25;
	film->set_video_frame_rate (24);
	stream->_frame_rate = 48000;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 50000);

	content->_video_frame_rate = 25;
	film->set_video_frame_rate (24);
	stream->_frame_rate = 44100;
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), 50000);

	/* Check some out-there conversions (not the best) */

	content->_video_frame_rate = 14.99;
	film->set_video_frame_rate (25);
	stream->_frame_rate = 16000;
	/* The FrameRateChange within resampled_frame_rate should choose to double-up
	   the 14.99 fps video to 30 and then run it slow at 25.
	*/
	BOOST_CHECK_EQUAL (content->audio->resampled_frame_rate(film), lrint (48000 * 2 * 14.99 / 25));
}

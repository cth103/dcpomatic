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
#include "lib/film.h"
#include "lib/config.h"
#include "lib/ffmpeg_content.h"
#include "lib/playlist.h"
#include "lib/frame_rate_change.h"
#include "test.h"

using boost::shared_ptr;

/* Test Playlist::best_dcp_frame_rate and FrameRateChange
   with a single piece of content.
*/
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test_single)
{
	shared_ptr<Film> film = new_test_film ("best_dcp_frame_rate_test_single");
	/* Get any piece of content, it doesn't matter what */
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/test.mp4"));
	film->add_content (content);
	wait_for_jobs ();
	
	/* Run some tests with a limited range of allowed rates */
	
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 60;
	int best = film->playlist()->best_dcp_frame_rate ();
	FrameRateChange frc = FrameRateChange (60, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	content->_video_frame_rate = 50;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (50, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 48;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (48, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 30;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (30, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 29.97;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (29.97, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	
	content->_video_frame_rate = 25;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (25, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 24;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (24, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 14.5;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (14.5, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	content->_video_frame_rate = 12.6;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (12.6, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	content->_video_frame_rate = 12.4;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (12.4, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	content->_video_frame_rate = 12;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (12, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Now add some more rates and see if it will use them
	   in preference to skip/repeat.
	*/

	afr.push_back (48);
	afr.push_back (50);
	afr.push_back (60);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 60;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (60, best);
	BOOST_CHECK_EQUAL (best, 60);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	content->_video_frame_rate = 50;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (50, best);
	BOOST_CHECK_EQUAL (best, 50);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	content->_video_frame_rate = 48;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (48, best);
	BOOST_CHECK_EQUAL (best, 48);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Check some out-there conversions (not the best) */
	
	frc = FrameRateChange (14.99, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 2);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	/* Check some conversions with limited DCP targets */

	afr.clear ();
	afr.push_back (24);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 25;
	best = film->playlist()->best_dcp_frame_rate ();
	frc = FrameRateChange (25, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, 1);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
}

/* Test Playlist::best_dcp_frame_rate and FrameRateChange
   with two pieces of content.
*/
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test_double)
{
	shared_ptr<Film> film = new_test_film ("best_dcp_frame_rate_test_double");
	/* Get any old content, it doesn't matter what */
	shared_ptr<FFmpegContent> A (new FFmpegContent (film, "test/data/test.mp4"));
	film->add_content (A);
	shared_ptr<FFmpegContent> B (new FFmpegContent (film, "test/data/test.mp4"));
	film->add_content (B);
	wait_for_jobs ();

	/* Run some tests with a limited range of allowed rates */
	
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	A->_video_frame_rate = 30;
	B->_video_frame_rate = 24;
	BOOST_CHECK_EQUAL (film->playlist()->best_dcp_frame_rate(), 25);
}


BOOST_AUTO_TEST_CASE (audio_sampling_rate_test)
{
	shared_ptr<Film> film = new_test_film ("audio_sampling_rate_test");
	/* Get any piece of content, it doesn't matter what */
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (content);
	wait_for_jobs ();
	
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	content->_video_frame_rate = 24;
	film->set_video_frame_rate (24);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 48000);

	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 48000);

	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 80000, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 96000);

	content->_video_frame_rate = 23.976;
	film->set_video_frame_rate (24);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 47952);

	content->_video_frame_rate = 29.97;
	film->set_video_frame_rate (30);
	BOOST_CHECK_EQUAL (film->video_frame_rate (), 30);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 47952);

	content->_video_frame_rate = 25;
	film->set_video_frame_rate (24);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 50000);

	content->_video_frame_rate = 25;
	film->set_video_frame_rate (24);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 50000);

	/* Check some out-there conversions (not the best) */
	
	content->_video_frame_rate = 14.99;
	film->set_video_frame_rate (25);
	content->set_audio_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream ("a", 42, 16000, 0)));
	/* The FrameRateChange within output_audio_frame_rate should choose to double-up
	   the 14.99 fps video to 30 and then run it slow at 25.
	*/
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), rint (48000 * 2 * 14.99 / 25));
}


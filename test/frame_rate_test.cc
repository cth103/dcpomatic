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

/* Test best_dcp_frame_rate and FrameRateConversion */
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test)
{
	/* Run some tests with a limited range of allowed rates */
	
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	int best = best_dcp_frame_rate (60);
	FrameRateConversion frc = FrameRateConversion (60, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (50);
	frc = FrameRateConversion (50, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (48);
	frc = FrameRateConversion (48, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (30);
	frc = FrameRateConversion (30, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (29.97);
	frc = FrameRateConversion (29.97, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	
	best = best_dcp_frame_rate (25);
	frc = FrameRateConversion (25, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (24);
	frc = FrameRateConversion (24, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (14.5);
	frc = FrameRateConversion (14.5, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12.6);
	frc = FrameRateConversion (12.6, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12.4);
	frc = FrameRateConversion (12.4, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12);
	frc = FrameRateConversion (12, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Now add some more rates and see if it will use them
	   in preference to skip/repeat.
	*/

	afr.push_back (48);
	afr.push_back (50);
	afr.push_back (60);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	best = best_dcp_frame_rate (60);
	frc = FrameRateConversion (60, best);
	BOOST_CHECK_EQUAL (best, 60);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (50);
	frc = FrameRateConversion (50, best);
	BOOST_CHECK_EQUAL (best, 50);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (48);
	frc = FrameRateConversion (48, best);
	BOOST_CHECK_EQUAL (best, 48);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Check some out-there conversions (not the best) */
	
	frc = FrameRateConversion (14.99, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	/* Check some conversions with limited DCP targets */

	afr.clear ();
	afr.push_back (24);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	best = best_dcp_frame_rate (25);
	frc = FrameRateConversion (25, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
}

BOOST_AUTO_TEST_CASE (audio_sampling_rate_test)
{
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	shared_ptr<Film> f = new_test_film ("audio_sampling_rate_test");
	f->set_source_frame_rate (24);
	f->set_dcp_frame_rate (24);

	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 48000);

	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 48000);

	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 80000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 96000);

	f->set_source_frame_rate (23.976);
	f->set_dcp_frame_rate (best_dcp_frame_rate (23.976));
	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 47952);

	f->set_source_frame_rate (29.97);
	f->set_dcp_frame_rate (best_dcp_frame_rate (29.97));
	BOOST_CHECK_EQUAL (f->dcp_frame_rate (), 30);
	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 47952);

	f->set_source_frame_rate (25);
	f->set_dcp_frame_rate (24);
	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 50000);

	f->set_source_frame_rate (25);
	f->set_dcp_frame_rate (24);
	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 50000);

	/* Check some out-there conversions (not the best) */
	
	f->set_source_frame_rate (14.99);
	f->set_dcp_frame_rate (25);
	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 16000, 0)));
	/* The FrameRateConversion within target_audio_sample_rate should choose to double-up
	   the 14.99 fps video to 30 and then run it slow at 25.
	*/
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), rint (48000 * 2 * 14.99 / 25));
}


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

using boost::shared_ptr;

shared_ptr<const Image> trimmer_test_last_video;
int trimmer_test_video_frames = 0;
shared_ptr<const AudioBuffers> trimmer_test_last_audio;

void
trimmer_test_video_helper (shared_ptr<const Image> image, bool, shared_ptr<Subtitle>)
{
	trimmer_test_last_video = image;
	++trimmer_test_video_frames;
}

void
trimmer_test_audio_helper (shared_ptr<const AudioBuffers> audio)
{
	trimmer_test_last_audio = audio;
}

BOOST_AUTO_TEST_CASE (trimmer_passthrough_test)
{
	Trimmer trimmer (shared_ptr<Log> (), 0, 0, 200, 48000, 25, 25);
	trimmer.Video.connect (bind (&trimmer_test_video_helper, _1, _2, _3));
	trimmer.Audio.connect (bind (&trimmer_test_audio_helper, _1));

	shared_ptr<SimpleImage> video (new SimpleImage (PIX_FMT_RGB24, libdcp::Size (1998, 1080), true));
	shared_ptr<AudioBuffers> audio (new AudioBuffers (6, 42 * 1920));

	trimmer.process_video (video, false, shared_ptr<Subtitle> ());
	trimmer.process_audio (audio);

	BOOST_CHECK_EQUAL (video.get(), trimmer_test_last_video.get());
	BOOST_CHECK_EQUAL (audio.get(), trimmer_test_last_audio.get());
	BOOST_CHECK_EQUAL (audio->frames(), trimmer_test_last_audio->frames());
}


/** Test the audio handling of the Trimmer */
BOOST_AUTO_TEST_CASE (trimmer_audio_test)
{
	Trimmer trimmer (shared_ptr<Log> (), 25, 75, 200, 48000, 25, 25);

	trimmer.Audio.connect (bind (&trimmer_test_audio_helper, _1));

	/* 21 video frames-worth of audio frames; should be completely stripped */
	trimmer_test_last_audio.reset ();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (6, 21 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio == 0);

	/* 42 more video frames-worth, 4 should be stripped from the start */
	audio.reset (new AudioBuffers (6, 42 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 38 * 1920);

	/* 42 more video frames-worth, should be kept as-is */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 42 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 42 * 1920);

	/* 25 more video frames-worth, 5 should be trimmed from the end */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 25 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 20 * 1920);

	/* Now some more; all should be trimmed */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 100 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio == 0);
}

BOOST_AUTO_TEST_CASE (trim_end_test)
{
	Trimmer trimmer (shared_ptr<Log> (), 0, 75, 200, 48000, 25, 25);

	shared_ptr<SimpleImage> image (new SimpleImage (PIX_FMT_RGB24, libdcp::Size (256, 256), true));

	trimmer.Video.connect (bind (&trimmer_test_video_helper, _1, _2, _3));
	trimmer_test_video_frames = 0;
	for (int i = 0; i < 200; ++i) {
		trimmer.process_video (image, false, shared_ptr<Subtitle> ());
	}

	BOOST_CHECK_EQUAL (trimmer_test_video_frames, 125);
}

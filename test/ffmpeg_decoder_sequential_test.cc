/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_decoder_sequential_test.cc
 *  @brief Check that the FFmpeg decoder produces sequential frames without gaps or dropped frames;
 *  (dropped frames being checked by assert() in VideoDecoder).  Also that the decoder picks up frame rates correctly.
 */

#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/null_log.h"
#include "lib/film.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::cout;
using std::cerr;
using std::list;
using boost::shared_ptr;
using boost::optional;

void
ffmpeg_decoder_sequential_test_one (boost::filesystem::path file, float fps, int gaps, int video_length)
{
	boost::filesystem::path path = private_data / file;
	if (!boost::filesystem::exists (path)) {
		cerr << "Skipping test: " << path.string() << " not found.\n";
		return;
	}

	shared_ptr<Film> film = new_test_film ("ffmpeg_decoder_seek_test_" + file.string());
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, path));
	film->examine_and_add_content (content);
	wait_for_jobs ();
	shared_ptr<Log> log (new NullLog);
	shared_ptr<FFmpegDecoder> decoder (new FFmpegDecoder (content, log, false));

	BOOST_REQUIRE (decoder->_video_content->video_frame_rate());
	BOOST_CHECK_CLOSE (decoder->_video_content->video_frame_rate().get(), fps, 0.01);

#ifdef DCPOMATIC_DEBUG
	decoder->test_gaps = 0;
#endif
	for (Frame i = 0; i < video_length; ++i) {
		list<ContentVideo> v;
		v = decoder->get_video (i, true);
		BOOST_REQUIRE_EQUAL (v.size(), 1U);
		BOOST_CHECK_EQUAL (v.front().frame, i);
	}
#ifdef DCPOMATIC_DEBUG
	BOOST_CHECK_EQUAL (decoder->test_gaps, gaps);
#endif
}

BOOST_AUTO_TEST_CASE (ffmpeg_decoder_sequential_test)
{
	ffmpeg_decoder_sequential_test_one ("boon_telly.mkv", 29.97, 0, 6910);
	ffmpeg_decoder_sequential_test_one ("Sintel_Trailer1.480p.DivX_Plus_HD.mkv", 24, 0, 1248);
	/* The first video frame is 12 here, so VideoDecoder should see 12 gaps
	   (at the start of the file)
	*/
	ffmpeg_decoder_sequential_test_one ("prophet_clip.mkv", 23.976, 12, 2875);
}

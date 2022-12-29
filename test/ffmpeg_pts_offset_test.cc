/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/ffmpeg_pts_offset_test.cc
 *  @brief Check the computation of _pts_offset in FFmpegDecoder.
 *  @ingroup selfcontained
 */


#include "lib/audio_content.h"
#include "lib/ffmpeg_audio_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (ffmpeg_pts_offset_test)
{
	auto film = new_test_film ("ffmpeg_pts_offset_test");
	auto content = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	content->audio = make_shared<AudioContent>(content.get());
	content->audio->add_stream (shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream));
	content->_video_frame_rate = 24;

	{
		/* Sound == video so no offset required */
		content->_first_video = ContentTime ();
		content->ffmpeg_audio_streams().front()->first_audio = ContentTime ();
		FFmpegDecoder decoder (film, content, false);
		BOOST_CHECK_EQUAL (decoder._pts_offset.get(), 0);
	}

	{
		/* Common offset should be removed */
		content->_first_video = ContentTime::from_seconds (600);
		content->ffmpeg_audio_streams().front()->first_audio = ContentTime::from_seconds (600);
		FFmpegDecoder decoder (film, content, false);
		BOOST_CHECK_EQUAL (decoder._pts_offset.get(), ContentTime::from_seconds(-600).get());
	}

	{
		/* Video is on a frame boundary */
		content->_first_video = ContentTime::from_frames (1, 24);
		content->ffmpeg_audio_streams().front()->first_audio = ContentTime ();
		FFmpegDecoder decoder (film, content, false);
		BOOST_CHECK_EQUAL (decoder._pts_offset.get(), 0);
	}

	{
		/* Video is off a frame boundary */
		double const frame = 1.0 / 24.0;
		content->_first_video = ContentTime::from_seconds (frame + 0.0215);
		content->ffmpeg_audio_streams().front()->first_audio = ContentTime ();
		FFmpegDecoder decoder (film, content, false);
		BOOST_CHECK_CLOSE (decoder._pts_offset.seconds(), (frame - 0.0215), 0.00001);
	}

	{
		/* Video is off a frame boundary and both have a common offset */
		double const frame = 1.0 / 24.0;
		content->_first_video = ContentTime::from_seconds (frame + 0.0215 + 4.1);
		content->ffmpeg_audio_streams().front()->first_audio = ContentTime::from_seconds (4.1);
		FFmpegDecoder decoder (film, content, false);
		BOOST_CHECK_CLOSE (decoder._pts_offset.seconds(), (frame - 0.0215) - 4.1, 0.1);
	}
}

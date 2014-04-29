/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_pts_offset_test.cc
 *  @brief Check the computation of _pts_offset in FFmpegDecoder.
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/ffmpeg_content.h"
#include "test.h"

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_pts_offset_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_pts_offset_test");
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/test.mp4"));
	content->_audio_stream.reset (new FFmpegAudioStream);
	content->_video_frame_rate = 24;

	{
		/* Sound == video so no offset required */
		content->_first_video = ContentTime ();
		content->_audio_stream->first_audio = ContentTime ();
		FFmpegDecoder decoder (content, film->log());
		BOOST_CHECK_EQUAL (decoder._pts_offset, ContentTime ());
	}

	{
		/* Common offset should be removed */
		content->_first_video = ContentTime::from_seconds (600);
		content->_audio_stream->first_audio = ContentTime::from_seconds (600);
		FFmpegDecoder decoder (content, film->log());
		BOOST_CHECK_EQUAL (decoder._pts_offset, ContentTime::from_seconds (-600));
	}

	{
		/* Video is on a frame boundary */
		content->_first_video = ContentTime::from_frames (1, 24);
		content->_audio_stream->first_audio = ContentTime ();
		FFmpegDecoder decoder (content, film->log());
		BOOST_CHECK_EQUAL (decoder._pts_offset, ContentTime ());
	}

	{
		/* Video is off a frame boundary */
		double const frame = 1.0 / 24.0;
		content->_first_video = ContentTime::from_seconds (frame + 0.0215);
		content->_audio_stream->first_audio = ContentTime ();
		FFmpegDecoder decoder (content, film->log());
		BOOST_CHECK_CLOSE (decoder._pts_offset.seconds(), (frame - 0.0215), 0.00001);
	}

	{
		/* Video is off a frame boundary and both have a common offset */
		double const frame = 1.0 / 24.0;
		content->_first_video = ContentTime::from_seconds (frame + 0.0215 + 4.1);
		content->_audio_stream->first_audio = ContentTime::from_seconds (4.1);
		FFmpegDecoder decoder (content, film->log());
		BOOST_CHECK_EQUAL (decoder._pts_offset.seconds(), (frame - 0.0215) - 4.1);
	}
}

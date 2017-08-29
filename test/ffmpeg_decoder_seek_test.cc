/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_decoder_seek_test.cc
 *  @brief Check seek() with FFmpegDecoder.
 *  @ingroup specific
 *
 *  This doesn't check that the contents of those frames are right, which
 *  it probably should.
 */

#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/null_log.h"
#include "lib/film.h"
#include "lib/content_video.h"
#include "lib/video_decoder.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <vector>
#include <iostream>

using std::cerr;
using std::vector;
using std::list;
using std::cout;
using boost::shared_ptr;
using boost::optional;

static optional<ContentVideo> stored;
static bool
store (ContentVideo v)
{
	stored = v;
	return true;
}

static void
check (shared_ptr<FFmpegDecoder> decoder, int frame)
{
	BOOST_REQUIRE (decoder->ffmpeg_content()->video_frame_rate ());
	decoder->seek (ContentTime::from_frames (frame, decoder->ffmpeg_content()->video_frame_rate().get()), true);
	stored = optional<ContentVideo> ();
	while (!decoder->pass() && !stored) {}
	BOOST_CHECK (stored->frame <= frame);
}

static void
test (boost::filesystem::path file, vector<int> frames)
{
	boost::filesystem::path path = private_data / file;
	BOOST_REQUIRE (boost::filesystem::exists (path));

	shared_ptr<Film> film = new_test_film ("ffmpeg_decoder_seek_test_" + file.string());
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, path));
	film->examine_and_add_content (content);
	wait_for_jobs ();
	shared_ptr<Log> log (new NullLog);
	shared_ptr<FFmpegDecoder> decoder (new FFmpegDecoder (content, log));
	decoder->video->Data.connect (bind (&store, _1));

	for (vector<int>::const_iterator i = frames.begin(); i != frames.end(); ++i) {
		check (decoder, *i);
	}
}

BOOST_AUTO_TEST_CASE (ffmpeg_decoder_seek_test)
{
	vector<int> frames;

	frames.clear ();
	frames.push_back (0);
	frames.push_back (42);
	frames.push_back (999);
	frames.push_back (0);

	test ("boon_telly.mkv", frames);
	test ("Sintel_Trailer1.480p.DivX_Plus_HD.mkv", frames);

	frames.clear ();
	frames.push_back (15);
	frames.push_back (42);
	frames.push_back (999);
	frames.push_back (15);

	test ("prophet_long_clip.mkv", frames);
}

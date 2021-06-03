/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup selfcontained
 *
 *  This doesn't check that the contents of those frames are right, which
 *  it probably should.
 */


#include "lib/content_video.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/film.h"
#include "lib/null_log.h"
#include "lib/video_decoder.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <vector>


using std::cerr;
using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


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
	auto path = TestPaths::private_data() / file;
	BOOST_REQUIRE (boost::filesystem::exists (path));

	auto film = new_test_film ("ffmpeg_decoder_seek_test_" + file.string());
	auto content = make_shared<FFmpegContent>(path);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	auto decoder = make_shared<FFmpegDecoder>(film, content, false);
	decoder->video->Data.connect (bind (&store, _1));

	for (auto i: frames) {
		check (decoder, i);
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_decoder_seek_test)
{
	vector<int> frames = { 0, 42, 999, 0 };

	test ("boon_telly.mkv", frames);
	test ("Sintel_Trailer1.480p.DivX_Plus_HD.mkv", frames);
	test ("prophet_long_clip.mkv", { 15, 42, 999, 15 });
}

/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_decoder_sequential_test.cc
 *  @brief Check that the FFmpeg decoder and Player produce sequential frames without gaps or dropped frames;
 *  Also that the decoder picks up frame rates correctly.
 *  @ingroup specific
 */

#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/null_log.h"
#include "lib/content_video.h"
#include "lib/video_decoder.h"
#include "lib/film.h"
#include "lib/player_video.h"
#include "lib/player.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::cout;
using std::cerr;
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::bind;

static DCPTime next;
static DCPTime frame;

static void
check (shared_ptr<PlayerVideo>, DCPTime time)
{
	BOOST_REQUIRE (time == next);
	next += frame;
}

void
ffmpeg_decoder_sequential_test_one (boost::filesystem::path file, float fps, int video_length)
{
	boost::filesystem::path path = private_data / file;
	BOOST_REQUIRE (boost::filesystem::exists (path));

	shared_ptr<Film> film = new_test_film ("ffmpeg_decoder_sequential_test_" + file.string());
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, path));
	film->examine_and_add_content (content);
	wait_for_jobs ();
	film->write_metadata ();
	shared_ptr<Log> log (new NullLog);
	shared_ptr<Player> player (new Player (film, film->playlist()));

	BOOST_REQUIRE (content->video_frame_rate());
	BOOST_CHECK_CLOSE (content->video_frame_rate().get(), fps, 0.01);

	player->Video.connect (bind (&check, _1, _2));

	next = DCPTime ();
	frame = DCPTime::from_frames (1, film->video_frame_rate ());
	while (!player->pass()) {}
	BOOST_REQUIRE (next == DCPTime::from_frames (video_length, film->video_frame_rate()));
}

BOOST_AUTO_TEST_CASE (ffmpeg_decoder_sequential_test)
{
	ffmpeg_decoder_sequential_test_one ("boon_telly.mkv", 29.97, 6912);
	ffmpeg_decoder_sequential_test_one ("Sintel_Trailer1.480p.DivX_Plus_HD.mkv", 24, 1253);
	ffmpeg_decoder_sequential_test_one ("prophet_clip.mkv", 23.976, 2879);
}

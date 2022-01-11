/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcpomatic_time.h"
#include "lib/player.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


/** @defgroup regression Regression tests */

/** @file  test/ffmpeg_decoder_erro_test.cc
 *  @brief Check some bugs in the FFmpegDecoder
 *  @ingroup regression
 */


BOOST_AUTO_TEST_CASE (check_exception_during_flush)
{
	auto content = content_factory(TestPaths::private_data() / "3d_thx_broadway_2010_lossless.m2ts").front();
	auto film = new_test_film2 ("check_exception_during_flush", { content });

	content->set_trim_start (dcpomatic::ContentTime(2310308));
	content->set_trim_end (dcpomatic::ContentTime(116020));

	make_and_verify_dcp (film);
}



BOOST_AUTO_TEST_CASE (check_exception_with_multiple_video_frames_per_packet)
{
	auto content = content_factory(TestPaths::private_data() / "chk.mkv").front();
	auto film = new_test_film2 ("check_exception_with_multiple_video_frames_per_packet", { content });
	auto player = std::make_shared<Player>(film, film->playlist());

	while (!player->pass()) {}
}


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


/** @file  test/repeat_frame_test.cc
 *  @brief Test the repeat of frames by the player when putting a 24fps
 *  source into a 48fps DCP.
 *  @ingroup feature
 *
 *  @see test/skip_frame_test.cc
 */


#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"


using std::make_shared;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (repeat_frame_test)
{
	auto c = make_shared<FFmpegContent>("test/data/red_24.mp4");
	auto film = new_test_film2 ("repeat_frame_test", {c});
	film->set_interop (false);
	c->video->set_custom_ratio (1.85);

	film->set_video_frame_rate (48);
	make_and_verify_dcp (film);

	/* Should be 32 frames of red followed by 16 frames of black to fill the DCP up to 1 second */
	check_dcp ("test/data/repeat_frame_test", film->dir (film->dcp_name ()));
}

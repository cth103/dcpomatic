/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
 *
 *  @see test/skip_frame_test.cc
 */

#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>

using boost::shared_ptr;
using boost::make_shared;

BOOST_AUTO_TEST_CASE (repeat_frame_test)
{
	shared_ptr<Film> film = new_test_film ("repeat_frame_test");
	film->set_name ("repeat_frame_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	shared_ptr<FFmpegContent> c = make_shared<FFmpegContent> (film, "test/data/red_24.mp4");
	film->examine_and_add_content (c);

	wait_for_jobs ();

	c->video->set_scale (VideoContentScale (Ratio::from_id ("185")));

	film->set_video_frame_rate (48);
	film->make_dcp ();
	wait_for_jobs ();

	/* Should be 32 frames of red */
	check_dcp ("test/data/repeat_frame_test", film->dir (film->dcp_name ()));
}

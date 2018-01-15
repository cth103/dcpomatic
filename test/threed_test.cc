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

/** @file  test/threed_test.cc
 *  @brief Create some 3D DCPs (without comparing the results to anything).
 *  @ingroup completedcp
 */

#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/video_content.h"
#include <iostream>

using std::cout;
using boost::shared_ptr;

/** Basic sanity check of 3D_LEFT_RIGHT */
BOOST_AUTO_TEST_CASE (threed_test1)
{
	shared_ptr<Film> film = new_test_film ("threed_test1");
	film->set_name ("test_film1");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	c->video->set_frame_type (VIDEO_FRAME_TYPE_3D_LEFT_RIGHT);
	c->video->set_scale (VideoContentScale (Ratio::from_id ("185")));

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Basic sanity check of 3D_ALTERNATE; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test2)
{
	shared_ptr<Film> film = new_test_film ("threed_test2");
	film->set_name ("test_film2");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (c);
	wait_for_jobs ();

	c->video->set_frame_type (VIDEO_FRAME_TYPE_3D_ALTERNATE);
	c->video->set_scale (VideoContentScale (Ratio::from_id ("185")));

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Basic sanity check of 3D_LEFT and 3D_RIGHT; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test3)
{
	shared_ptr<Film> film = new_test_film2 ("threed_test3");
	shared_ptr<FFmpegContent> L (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (L);
	shared_ptr<FFmpegContent> R (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (R);
	wait_for_jobs ();

	L->video->set_frame_type (VIDEO_FRAME_TYPE_3D_LEFT);
	R->video->set_frame_type (VIDEO_FRAME_TYPE_3D_RIGHT);

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

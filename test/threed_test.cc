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

/** @file  test/threed_test.cc
 *  @brief Create a 3D DCP (without comparing the result to anything).
 */

#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"

using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (threed_test)
{
	shared_ptr<Film> film = new_test_film ("threed_test");
	film->set_name ("test_film2");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	c->set_video_frame_type (VIDEO_FRAME_TYPE_3D_LEFT_RIGHT);
	film->examine_and_add_content (c, true);

	wait_for_jobs ();

	c->set_scale (VideoContentScale (Ratio::from_id ("185")));
	
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	wait_for_jobs ();
}

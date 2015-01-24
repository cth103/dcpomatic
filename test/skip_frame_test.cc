/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/skip_frame_test.cc
 *  @brief Test the skip of frames by the player when putting a 48fps
 *  source into a 24fps DCP.
 *
 *  @see test/repeat_frame_test.cc
 */

#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (skip_frame_test)
{
	shared_ptr<Film> film = new_test_film ("skip_frame_test");
	film->set_name ("skip_frame_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/count300bd48.m2ts"));
	film->examine_and_add_content (c);

	wait_for_jobs ();

	c->set_scale (VideoContentScale (Ratio::from_id ("185")));
	film->write_metadata ();

	film->set_video_frame_rate (24);
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/skip_frame_test", film->dir (film->dcp_name ()));
}


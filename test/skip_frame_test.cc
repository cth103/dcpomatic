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


/** @file  test/skip_frame_test.cc
 *  @brief Test the skip of frames by the player when putting a 48fps
 *  source into a 24fps DCP.
 *  @ingroup feature
 *
 *  @see test/repeat_frame_test.cc
 */


#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"


using std::make_shared;


BOOST_AUTO_TEST_CASE (skip_frame_test)
{
	auto film = new_test_film ("skip_frame_test");
	film->set_name ("skip_frame_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_interop (false);
	auto c = make_shared<FFmpegContent>("test/data/count300bd48.m2ts");
	film->examine_and_add_content (c);

	BOOST_REQUIRE (!wait_for_jobs());

	film->write_metadata ();

	film->set_video_frame_rate (24);
	make_and_verify_dcp (film);

	/* Should be white numbers on a black background counting up from 2 in steps of 2
	   up to 300.
	*/
	check_dcp ("test/data/skip_frame_test", film->dir (film->dcp_name ()));
}

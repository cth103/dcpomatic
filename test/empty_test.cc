/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "lib/image_content.h"
#include "lib/empty.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (empty_test1)
{
	shared_ptr<Film> film = new_test_film ("empty_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("empty_test1");
	film->set_container (Ratio::from_id ("185"));
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent (film, "test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	wait_for_jobs ();

	int const vfr = film->video_frame_rate ();

	contentA->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentA->video->set_length (3);
	contentA->set_position (DCPTime::from_frames (2, vfr));
	contentB->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentB->video->set_length (1);
	contentB->set_position (DCPTime::from_frames (7, vfr));

	Empty black (film->content(), film->length(), bind(&Content::video, _1));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 2);
	BOOST_CHECK (black._periods.front().from == DCPTime());
	BOOST_CHECK (black._periods.front().to == DCPTime::from_frames(2, vfr));
	BOOST_CHECK (black._periods.back().from == DCPTime::from_frames(5, vfr));
	BOOST_CHECK (black._periods.back().to == DCPTime::from_frames(7, vfr));
}

/** Some tests where the first empty period is not at time 0 */
BOOST_AUTO_TEST_CASE (empty_test2)
{
	shared_ptr<Film> film = new_test_film ("empty_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("empty_test1");
	film->set_container (Ratio::from_id ("185"));
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent (film, "test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	wait_for_jobs ();

	int const vfr = film->video_frame_rate ();

	contentA->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentA->video->set_length (3);
	contentA->set_position (DCPTime(0));
	contentB->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentB->video->set_length (1);
	contentB->set_position (DCPTime::from_frames (7, vfr));

	Empty black (film->content(), film->length(), bind(&Content::video, _1));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 1);
	BOOST_CHECK (black._periods.front().from == DCPTime::from_frames(3, vfr));
	BOOST_CHECK (black._periods.front().to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (black.position() == DCPTime::from_frames(3, vfr));

	/* check that done() works */
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (4, vfr));
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (7, vfr));
	BOOST_CHECK (black.done ());
}

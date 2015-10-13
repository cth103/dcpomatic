/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::list;
using boost::shared_ptr;

/** Test Film::reels() */
BOOST_AUTO_TEST_CASE (reels_test1)
{
	shared_ptr<Film> film = new_test_film ("reels_test1");
	film->set_container (Ratio::from_id ("185"));
	shared_ptr<FFmpegContent> A (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (A);
	shared_ptr<FFmpegContent> B (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (B);
	wait_for_jobs ();
	BOOST_CHECK_EQUAL (A->full_length(), DCPTime (288000));

	film->set_reel_type (REELTYPE_SINGLE);
	list<DCPTimePeriod> r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 1);
	BOOST_CHECK_EQUAL (r.front().from, DCPTime (0));
	BOOST_CHECK_EQUAL (r.front().to, DCPTime (288000 * 2));

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 2);
	BOOST_CHECK_EQUAL (r.front().from, DCPTime (0));
	BOOST_CHECK_EQUAL (r.front().to, DCPTime (288000));
	BOOST_CHECK_EQUAL (r.back().from, DCPTime (288000));
	BOOST_CHECK_EQUAL (r.back().to, DCPTime (288000 * 2));

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_CHECK_EQUAL (i->from, DCPTime (0));
	BOOST_CHECK_EQUAL (i->to, DCPTime::from_frames (60, 24));
	++i;
	BOOST_CHECK_EQUAL (i->from, DCPTime::from_frames (60, 24));
	BOOST_CHECK_EQUAL (i->to, DCPTime::from_frames (120, 24));
	++i;
	BOOST_CHECK_EQUAL (i->from, DCPTime::from_frames (120, 24));
	BOOST_CHECK_EQUAL (i->to, DCPTime::from_frames (144, 24));
}

/** Make a short DCP with multi reels split by video content, then import
 *  this into a new project and make a new DCP referencing it.
 */
BOOST_AUTO_TEST_CASE (reels_test2)
{
	shared_ptr<Film> film = new_test_film ("reels_test2");
	film->set_name ("reels_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_red.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->set_video_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_green.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->set_video_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_blue.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->set_video_length (24);
	}

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	wait_for_jobs ();

	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/reels_test2", film->dir (film->dcp_name()));

	shared_ptr<Film> film2 = new_test_film ("reels_test3");
	film2->set_name ("reels_test3");
	film2->set_container (Ratio::from_id ("185"));
	film2->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film2->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<DCPContent> c (new DCPContent (film2, film->dir (film->dcp_name ())));
	film2->examine_and_add_content (c);
	wait_for_jobs ();

	list<DCPTimePeriod> r = film2->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_CHECK_EQUAL (i->from, DCPTime (0));
	BOOST_CHECK_EQUAL (i->to, DCPTime (96000));
	++i;
	BOOST_CHECK_EQUAL (i->from, DCPTime (96000));
	BOOST_CHECK_EQUAL (i->to, DCPTime (96000 * 2));
	++i;
	BOOST_CHECK_EQUAL (i->from, DCPTime (96000 * 2));
	BOOST_CHECK_EQUAL (i->to, DCPTime (96000 * 3));

	c->set_reference_video (true);
	c->set_reference_audio (true);

	film2->make_dcp ();
	wait_for_jobs ();
}

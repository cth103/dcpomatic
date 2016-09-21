/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_content.h"
#include "lib/video_content.h"
#include "lib/text_subtitle_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

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
	BOOST_CHECK_EQUAL (A->full_length().get(), 288000);

	film->set_reel_type (REELTYPE_SINGLE);
	list<DCPTimePeriod> r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 1);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000 * 2);

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 2);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().from.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().to.get(), 288000 * 2);

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_CHECK_EQUAL (i->from.get(), 0);
	BOOST_CHECK_EQUAL (i->to.get(), DCPTime::from_frames(60, 24).get());
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), DCPTime::from_frames(60, 24).get());
	BOOST_CHECK_EQUAL (i->to.get(), DCPTime::from_frames(120, 24).get());
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), DCPTime::from_frames(120, 24).get());
	BOOST_CHECK_EQUAL (i->to.get(), DCPTime::from_frames(144, 24).get());
}

/** Make a short DCP with multi reels split by video content, then import
 *  this into a new project and make a new DCP referencing it.
 */
BOOST_AUTO_TEST_CASE (reels_test2)
{
	shared_ptr<Film> film = new_test_film ("reels_test2");
	film->set_name ("reels_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_red.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->video->set_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_green.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->video->set_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent (film, "test/data/flat_blue.png"));
		film->examine_and_add_content (c);
		wait_for_jobs ();
		c->video->set_length (24);
	}

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	wait_for_jobs ();

	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/reels_test2", film->dir (film->dcp_name()));

	shared_ptr<Film> film2 = new_test_film ("reels_test2b");
	film2->set_name ("reels_test2b");
	film2->set_container (Ratio::from_id ("185"));
	film2->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film2->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<DCPContent> c (new DCPContent (film2, film->dir (film->dcp_name ())));
	film2->examine_and_add_content (c);
	wait_for_jobs ();

	list<DCPTimePeriod> r = film2->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3);
	list<DCPTimePeriod>::const_iterator i = r.begin ();
	BOOST_CHECK_EQUAL (i->from.get(), 0);
	BOOST_CHECK_EQUAL (i->to.get(), 96000);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 2);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000 * 2);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 3);

	c->set_reference_video (true);
	c->set_reference_audio (true);

	film2->make_dcp ();
	wait_for_jobs ();
}

/** Check that REELTYPE_BY_VIDEO_CONTENT adds an extra reel, if necessary, at the end
 *  of all the video content to mop up anything afterward.
 */
BOOST_AUTO_TEST_CASE (reels_test3)
{
	shared_ptr<Film> film = new_test_film ("reels_test3");
	film->set_name ("reels_test3");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<Content> dcp (new DCPContent (film, "test/data/reels_test2"));
	film->examine_and_add_content (dcp);
	shared_ptr<Content> sub (new TextSubtitleContent (film, "test/data/subrip.srt"));
	film->examine_and_add_content (sub);
	wait_for_jobs ();

	list<DCPTimePeriod> reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4);
	list<DCPTimePeriod>::const_iterator i = reels.begin ();
	BOOST_CHECK_EQUAL (i->from.get(), 0);
	BOOST_CHECK_EQUAL (i->to.get(), 96000);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 2);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000 * 2);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 3);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000 * 3);
	BOOST_CHECK_EQUAL (i->to.get(), sub->full_length().round_up(film->video_frame_rate()).get());
}

/** Check creation of a multi-reel DCP with a single .srt subtitle file;
 *  make sure that the reel subtitle timing is done right.
 */
BOOST_AUTO_TEST_CASE (reels_test4)
{
	shared_ptr<Film> film = new_test_film ("reels_test4");
	film->set_name ("reels_test4");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	/* 4 piece of 1s-long content */
	shared_ptr<ImageContent> content[4];
	for (int i = 0; i < 4; ++i) {
		content[i].reset (new ImageContent (film, "test/data/flat_green.png"));
		film->examine_and_add_content (content[i]);
		wait_for_jobs ();
		content[i]->video->set_length (24);
	}

	shared_ptr<TextSubtitleContent> subs (new TextSubtitleContent (film, "test/data/subrip3.srt"));
	film->examine_and_add_content (subs);
	wait_for_jobs ();

	list<DCPTimePeriod> reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4);
	list<DCPTimePeriod>::const_iterator i = reels.begin ();
	BOOST_CHECK_EQUAL (i->from.get(), 0);
	BOOST_CHECK_EQUAL (i->to.get(), 96000);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 2);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000 * 2);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 3);
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), 96000 * 3);
	BOOST_CHECK_EQUAL (i->to.get(), 96000 * 4);

	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/reels_test4", film->dir (film->dcp_name()));
}

BOOST_AUTO_TEST_CASE (reels_test5)
{
	shared_ptr<Film> film = new_test_film ("reels_test4");
	shared_ptr<DCPContent> dcp (new DCPContent (film, "test/data/reels_test4"));
	film->examine_and_add_content (dcp);
	wait_for_jobs ();

	dcp->set_position(DCPTime(123));

	{
		list<DCPTimePeriod> p = dcp->reels ();
		BOOST_REQUIRE_EQUAL (p.size(), 4);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 0), DCPTime(123 + 96000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 96000), DCPTime(123 + 192000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 192000), DCPTime(123 + 288000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 288000), DCPTime(123 + 384000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (0.5));
		list<DCPTimePeriod> p = dcp->reels ();
		BOOST_REQUIRE_EQUAL (p.size(), 4);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 0), DCPTime(123 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 48000), DCPTime(123 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 144000), DCPTime(123 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 240000), DCPTime(123 + 336000)));
	}

	{
		dcp->set_trim_end (ContentTime::from_seconds (0.5));
		list<DCPTimePeriod> p = dcp->reels ();
		BOOST_REQUIRE_EQUAL (p.size(), 4);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 0), DCPTime(123 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 48000), DCPTime(123 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 144000), DCPTime(123 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 240000), DCPTime(123 + 288000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (1.5));
		list<DCPTimePeriod> p = dcp->reels ();
		BOOST_REQUIRE_EQUAL (p.size(), 3);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 0), DCPTime(123 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 48000), DCPTime(123 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(123 + 144000), DCPTime(123 + 192000)));
	}
}

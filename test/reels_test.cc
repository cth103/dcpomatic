/*
    Copyright (C) 2015-2020 Carl Hetherington <cth@carlh.net>

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

/** @file  test/reels_test.cc
 *  @brief Check manipulation of reels in various ways.
 *  @ingroup specific
 */

#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_content.h"
#include "lib/video_content.h"
#include "lib/string_text_file_content.h"
#include "lib/content_factory.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::cout;
using std::vector;
using std::string;
using boost::shared_ptr;
using boost::function;
using namespace dcpomatic;

/** Test Film::reels() */
BOOST_AUTO_TEST_CASE (reels_test1)
{
	shared_ptr<Film> film = new_test_film ("reels_test1");
	film->set_container (Ratio::from_id ("185"));
	shared_ptr<FFmpegContent> A (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (A);
	shared_ptr<FFmpegContent> B (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK_EQUAL (A->full_length(film).get(), 288000);

	film->set_reel_type (REELTYPE_SINGLE);
	list<DCPTimePeriod> r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 1U);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000 * 2);

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 2U);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().from.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().to.get(), 288000 * 2);

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3U);
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
	film->set_interop (false);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));

	{
		shared_ptr<ImageContent> c (new ImageContent("test/data/flat_red.png"));
		film->examine_and_add_content (c);
		BOOST_REQUIRE (!wait_for_jobs());
		c->video->set_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent("test/data/flat_green.png"));
		film->examine_and_add_content (c);
		BOOST_REQUIRE (!wait_for_jobs());
		c->video->set_length (24);
	}

	{
		shared_ptr<ImageContent> c (new ImageContent("test/data/flat_blue.png"));
		film->examine_and_add_content (c);
		BOOST_REQUIRE (!wait_for_jobs());
		c->video->set_length (24);
	}

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	BOOST_CHECK_EQUAL (film->reels().size(), 3U);
	BOOST_REQUIRE (!wait_for_jobs());

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	check_dcp ("test/data/reels_test2", film->dir (film->dcp_name()));

	shared_ptr<Film> film2 = new_test_film ("reels_test2b");
	film2->set_name ("reels_test2b");
	film2->set_container (Ratio::from_id ("185"));
	film2->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film2->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<DCPContent> c (new DCPContent(film->dir(film->dcp_name())));
	film2->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	list<DCPTimePeriod> r = film2->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3U);
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
	BOOST_REQUIRE (!wait_for_jobs());
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

	shared_ptr<Content> dcp (new DCPContent("test/data/reels_test2"));
	film->examine_and_add_content (dcp);
	shared_ptr<Content> sub (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (sub);
	BOOST_REQUIRE (!wait_for_jobs());

	list<DCPTimePeriod> reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4U);
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
	BOOST_CHECK_EQUAL (i->to.get(), sub->full_length(film).ceil(film->video_frame_rate()).get());
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
	film->set_interop (false);

	/* 4 piece of 1s-long content */
	shared_ptr<ImageContent> content[4];
	for (int i = 0; i < 4; ++i) {
		content[i].reset (new ImageContent("test/data/flat_green.png"));
		film->examine_and_add_content (content[i]);
		BOOST_REQUIRE (!wait_for_jobs());
		content[i]->video->set_length (24);
	}

	shared_ptr<StringTextFileContent> subs (new StringTextFileContent("test/data/subrip3.srt"));
	film->examine_and_add_content (subs);
	BOOST_REQUIRE (!wait_for_jobs());

	list<DCPTimePeriod> reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4U);
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
	BOOST_REQUIRE (!wait_for_jobs());

	check_dcp ("test/data/reels_test4", film->dir (film->dcp_name()));
}

BOOST_AUTO_TEST_CASE (reels_test5)
{
	shared_ptr<Film> film = new_test_film ("reels_test5");
	film->set_sequence (false);
	shared_ptr<DCPContent> dcp (new DCPContent("test/data/reels_test4"));
	film->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Set to 2123 but it will be rounded up to the next frame (4000) */
	dcp->set_position(film, DCPTime(2123));

	{
		list<DCPTimePeriod> p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 96000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 96000), DCPTime(4000 + 192000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 192000), DCPTime(4000 + 288000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 288000), DCPTime(4000 + 384000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (0.5));
		list<DCPTimePeriod> p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 240000), DCPTime(4000 + 336000)));
	}

	{
		dcp->set_trim_end (ContentTime::from_seconds (0.5));
		list<DCPTimePeriod> p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 240000), DCPTime(4000 + 288000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (1.5));
		list<DCPTimePeriod> p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 3U);
		list<DCPTimePeriod>::const_iterator i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 192000)));
	}
}

/** Check reel split with a muxed video/audio source */
BOOST_AUTO_TEST_CASE (reels_test6)
{
	shared_ptr<Film> film = new_test_film ("reels_test6");
	film->set_name ("reels_test6");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	shared_ptr<FFmpegContent> A (new FFmpegContent("test/data/test2.mp4"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs ());

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Check the case where the last bit of audio hangs over the end of the video
 *  and we are using REELTYPE_BY_VIDEO_CONTENT.
 */
BOOST_AUTO_TEST_CASE (reels_test7)
{
	shared_ptr<Film> film = new_test_film ("reels_test7");
	film->set_name ("reels_test7");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	shared_ptr<Content> A = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs ());
	shared_ptr<Content> B = content_factory("test/data/awkward_length.wav").front();
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->set_video_frame_rate (24);
	A->video->set_length (2 * 24);

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL (film->reels().size(), 2U);
	BOOST_CHECK (film->reels().front() == DCPTimePeriod(DCPTime(0), DCPTime::from_frames(2 * 24, 24)));
	BOOST_CHECK (film->reels().back() == DCPTimePeriod(DCPTime::from_frames(2 * 24, 24), DCPTime::from_frames(3 * 24 + 1, 24)));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Check a reels-related error; make_dcp() would raise a ProgrammingError */
BOOST_AUTO_TEST_CASE (reels_test8)
{
	shared_ptr<Film> film = new_test_film ("reels_test8");
	film->set_name ("reels_test8");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	shared_ptr<FFmpegContent> A (new FFmpegContent("test/data/test2.mp4"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs ());

	A->set_trim_end (ContentTime::from_seconds (1));
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Check another reels-related error; make_dcp() would raise a ProgrammingError */
BOOST_AUTO_TEST_CASE (reels_test9)
{
	shared_ptr<Film> film = new_test_film2("reels_test9a");
	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content(A);
	BOOST_REQUIRE(!wait_for_jobs());
	A->video->set_length(5 * 24);
	film->set_video_frame_rate(24);
	film->make_dcp();
	BOOST_REQUIRE(!wait_for_jobs());

	shared_ptr<Film> film2 = new_test_film2("reels_test9b");
	shared_ptr<DCPContent> B(new DCPContent(film->dir(film->dcp_name())));
	film2->examine_and_add_content(B);
	film2->examine_and_add_content(content_factory("test/data/dcp_sub4.xml").front());
	B->set_reference_video(true);
	B->set_reference_audio(true);
	BOOST_REQUIRE(!wait_for_jobs());
	film2->set_reel_type(REELTYPE_BY_VIDEO_CONTENT);
	film2->write_metadata();
	film2->make_dcp();
	BOOST_REQUIRE(!wait_for_jobs());
}

/** Another reels-related error; make_dcp() would raise a ProgrammingError
 *  in AudioBuffers::allocate due to an attempt to allocate a negatively-sized buffer.
 *  This was triggered by a VF where there are referenced audio reels followed by
 *  VF audio.  When the VF audio arrives the Writer did not correctly skip over the
 *  referenced reels.
 */
BOOST_AUTO_TEST_CASE (reels_test10)
{
	/* Make the OV */
	shared_ptr<Film> ov = new_test_film2("reels_test10_ov");
	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	ov->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (5 * 24);

	shared_ptr<FFmpegContent> B(new FFmpegContent("test/data/flat_red.png"));
	ov->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	B->video->set_length (5 * 24);

	ov->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	ov->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
	ov->write_metadata ();

	/* Now try to make the VF; this used to fail */
	shared_ptr<Film> vf = new_test_film2("reels_test10_vf");
	shared_ptr<DCPContent> ov_dcp(new DCPContent(ov->dir(ov->dcp_name())));
	vf->examine_and_add_content (ov_dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	vf->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video (true);
	ov_dcp->set_reference_audio (true);
	vf->examine_and_add_content (content_factory("test/data/15s.srt").front());
	BOOST_REQUIRE (!wait_for_jobs());

	vf->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
	vf->write_metadata ();
}

/** Another reels error; REELTYPE_BY_VIDEO_CONTENT when the first content is not
 *  at time 0.
 */
BOOST_AUTO_TEST_CASE (reels_test11)
{
	shared_ptr<Film> film = new_test_film2 ("reels_test11");
	film->set_video_frame_rate (24);
	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (240);
	A->set_video_frame_rate (24);
	A->set_position (film, DCPTime::from_seconds(1));
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK_EQUAL (A->position().get(), DCPTime::from_seconds(1).get());
	BOOST_CHECK_EQUAL (A->end(film).get(), DCPTime::from_seconds(1 + 10).get());

	list<DCPTimePeriod> r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 2U);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), DCPTime::from_seconds(1).get());
	BOOST_CHECK_EQUAL (r.back().from.get(), DCPTime::from_seconds(1).get());
	BOOST_CHECK_EQUAL (r.back().to.get(), DCPTime::from_seconds(1 + 10).get());
}

/** For VFs to work right we have to make separate reels for empty bits between
 *  video content.
 */
BOOST_AUTO_TEST_CASE (reels_test12)
{
	shared_ptr<Film> film = new_test_film2 ("reels_test12");
	film->set_video_frame_rate (24);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	film->set_sequence (false);

	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (240);
	A->set_video_frame_rate (24);
	A->set_position (film, DCPTime::from_seconds(1));

	shared_ptr<FFmpegContent> B(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	B->video->set_length (120);
	B->set_video_frame_rate (24);
	B->set_position (film, DCPTime::from_seconds(14));

	list<DCPTimePeriod> r = film->reels ();
	BOOST_REQUIRE_EQUAL (r.size(), 4U);
	list<DCPTimePeriod>::const_iterator i = r.begin ();

	BOOST_CHECK_EQUAL (i->from.get(), 0);
	BOOST_CHECK_EQUAL (i->to.get(),   DCPTime::from_seconds(1).get());
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), DCPTime::from_seconds(1).get());
	BOOST_CHECK_EQUAL (i->to.get(),   DCPTime::from_seconds(11).get());
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), DCPTime::from_seconds(11).get());
	BOOST_CHECK_EQUAL (i->to.get(),   DCPTime::from_seconds(14).get());
	++i;
	BOOST_CHECK_EQUAL (i->from.get(), DCPTime::from_seconds(14).get());
	BOOST_CHECK_EQUAL (i->to.get(),   DCPTime::from_seconds(19).get());
}

static void
no_op ()
{

}

static void
dump_notes (list<dcp::VerificationNote> const & notes)
{
	BOOST_FOREACH (dcp::VerificationNote i, notes) {
		std::cout << dcp::note_to_string(i) << "\n";
	}
}


/** Using less than 1 second's worth of content should not result in a reel
 *  of less than 1 second's duration.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short1)
{
	shared_ptr<Film> film = new_test_film2 ("reels_should_not_be_short1");
	film->set_video_frame_rate (24);

	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (23);

	shared_ptr<FFmpegContent> B(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	B->video->set_length (23);
	B->set_position (film, DCPTime::from_frames(23, 24));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	vector<boost::filesystem::path> dirs;
	dirs.push_back (film->dir(film->dcp_name(false)));
	list<dcp::VerificationNote> const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}

/** Leaving less than 1 second's gap between two pieces of content with
 *  REELTYPE_BY_VIDEO_CONTENT should not make a <1s reel.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short2)
{
	shared_ptr<Film> film = new_test_film2 ("reels_should_not_be_short2");
	film->set_video_frame_rate (24);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (240);

	shared_ptr<FFmpegContent> B(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	B->video->set_length (240);
	B->set_position (film, DCPTime::from_seconds(10.2));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	vector<boost::filesystem::path> dirs;
	dirs.push_back (film->dir(film->dcp_name(false)));
	list<dcp::VerificationNote> const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}

/** Setting REELTYPE_BY_LENGTH and using a small length value should not make
 *  <1s reels.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short3)
{
	shared_ptr<Film> film = new_test_film2 ("reels_should_not_be_short3");
	film->set_video_frame_rate (24);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	film->set_reel_length (1024 * 1024 * 10);

	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (240);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	vector<boost::filesystem::path> dirs;
	list<dcp::VerificationNote> const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}

/** Having one piece of content less than 1s long in REELTYPE_BY_VIDEO_CONTENT
 *  should not make a reel less than 1s long.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short4)
{
	shared_ptr<Film> film = new_test_film2 ("reels_should_not_be_short4");
	film->set_video_frame_rate (24);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	shared_ptr<FFmpegContent> A(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs());
	A->video->set_length (240);

	shared_ptr<FFmpegContent> B(new FFmpegContent("test/data/flat_red.png"));
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	B->video->set_length (23);
	B->set_position (film, DCPTime::from_frames(240, 24));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	vector<boost::filesystem::path> dirs;
	dirs.push_back (film->dir(film->dcp_name(false)));
	list<dcp::VerificationNote> const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}


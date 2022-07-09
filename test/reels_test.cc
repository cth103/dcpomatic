/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/make_dcp.h"
#include "lib/ratio.h"
#include "lib/string_text_file_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::function;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;


/** Test Film::reels() */
BOOST_AUTO_TEST_CASE (reels_test1)
{
	auto film = new_test_film ("reels_test1");
	film->set_container (Ratio::from_id ("185"));
	auto A = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (A);
	auto B = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK_EQUAL (A->full_length(film).get(), 288000);

	film->set_reel_type (ReelType::SINGLE);
	auto r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 1U);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000 * 2);

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 2U);
	BOOST_CHECK_EQUAL (r.front().from.get(), 0);
	BOOST_CHECK_EQUAL (r.front().to.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().from.get(), 288000);
	BOOST_CHECK_EQUAL (r.back().to.get(), 288000 * 2);

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (ReelType::BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	r = film->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3U);
	auto i = r.begin ();
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
	auto film = new_test_film ("reels_test2");
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

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	BOOST_CHECK_EQUAL (film->reels().size(), 3U);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (film);

	check_dcp ("test/data/reels_test2", film->dir (film->dcp_name()));

	auto c = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2 ("reels_test2b", {c});
	film2->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	auto r = film2->reels ();
	BOOST_CHECK_EQUAL (r.size(), 3U);
	auto i = r.begin ();
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

	make_and_verify_dcp (film2, {dcp::VerificationNote::Code::EXTERNAL_ASSET});
}


/** Check that ReelType::BY_VIDEO_CONTENT adds an extra reel, if necessary, at the end
 *  of all the video content to mop up anything afterward.
 */
BOOST_AUTO_TEST_CASE (reels_test3)
{
	auto dcp = make_shared<DCPContent>("test/data/reels_test2");
	auto sub = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film2 ("reels_test3", {dcp, sub});
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	auto reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4U);
	auto i = reels.begin ();
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
	auto film = new_test_film2 ("reels_test4");
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	film->set_interop (false);

	/* 4 piece of 1s-long content */
	shared_ptr<ImageContent> content[4];
	for (int i = 0; i < 4; ++i) {
		content[i].reset (new ImageContent("test/data/flat_green.png"));
		film->examine_and_add_content (content[i]);
		BOOST_REQUIRE (!wait_for_jobs());
		content[i]->video->set_length (24);
	}

	auto subs = make_shared<StringTextFileContent>("test/data/subrip3.srt");
	film->examine_and_add_content (subs);
	BOOST_REQUIRE (!wait_for_jobs());

	auto reels = film->reels();
	BOOST_REQUIRE_EQUAL (reels.size(), 4U);
	auto i = reels.begin ();
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

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION
		});

	check_dcp ("test/data/reels_test4", film->dir (film->dcp_name()));
}


BOOST_AUTO_TEST_CASE (reels_test5)
{
	auto dcp = make_shared<DCPContent>("test/data/reels_test4");
	dcp->check_font_ids();
	auto film = new_test_film2 ("reels_test5", {dcp});
	film->set_sequence (false);

	/* Set to 2123 but it will be rounded up to the next frame (4000) */
	dcp->set_position(film, DCPTime(2123));

	{
		auto p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		auto i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 96000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 96000), DCPTime(4000 + 192000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 192000), DCPTime(4000 + 288000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 288000), DCPTime(4000 + 384000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (0.5));
		auto p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		auto i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 240000), DCPTime(4000 + 336000)));
	}

	{
		dcp->set_trim_end (ContentTime::from_seconds (0.5));
		auto p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 4U);
		auto i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 240000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 240000), DCPTime(4000 + 288000)));
	}

	{
		dcp->set_trim_start (ContentTime::from_seconds (1.5));
		auto p = dcp->reels (film);
		BOOST_REQUIRE_EQUAL (p.size(), 3U);
		auto i = p.begin();
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 0), DCPTime(4000 + 48000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 48000), DCPTime(4000 + 144000)));
		BOOST_CHECK (*i++ == DCPTimePeriod (DCPTime(4000 + 144000), DCPTime(4000 + 192000)));
	}
}


/** Check reel split with a muxed video/audio source */
BOOST_AUTO_TEST_CASE (reels_test6)
{
	auto A = make_shared<FFmpegContent>("test/data/test2.mp4");
	auto film = new_test_film2 ("reels_test6", {A});

	film->set_j2k_bandwidth (100000000);
	film->set_reel_type (ReelType::BY_LENGTH);
	/* This is just over 2.5s at 100Mbit/s; should correspond to 60 frames */
	film->set_reel_length (31253154);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION,
			dcp::VerificationNote::Code::INVALID_DURATION,
		});
}


/** Check the case where the last bit of audio hangs over the end of the video
 *  and we are using ReelType::BY_VIDEO_CONTENT.
 */
BOOST_AUTO_TEST_CASE (reels_test7)
{
	auto A = content_factory("test/data/flat_red.png")[0];
	auto B = content_factory("test/data/awkward_length.wav")[0];
	auto film = new_test_film2 ("reels_test7", { A, B });
	film->set_video_frame_rate (24);
	A->video->set_length (2 * 24);

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL (film->reels().size(), 2U);
	BOOST_CHECK (film->reels().front() == DCPTimePeriod(DCPTime(0), DCPTime::from_frames(2 * 24, 24)));
	BOOST_CHECK (film->reels().back() == DCPTimePeriod(DCPTime::from_frames(2 * 24, 24), DCPTime::from_frames(3 * 24 + 1, 24)));

	make_and_verify_dcp (film);
}


/** Check a reels-related error; make_dcp() would raise a ProgrammingError */
BOOST_AUTO_TEST_CASE (reels_test8)
{
	auto A = make_shared<FFmpegContent>("test/data/test2.mp4");
	auto film = new_test_film2 ("reels_test8", {A});

	A->set_trim_end (ContentTime::from_seconds (1));
	make_and_verify_dcp (film);
}


/** Check another reels-related error; make_dcp() would raise a ProgrammingError */
BOOST_AUTO_TEST_CASE (reels_test9)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2("reels_test9a", {A});
	A->video->set_length(5 * 24);
	film->set_video_frame_rate(24);
	make_and_verify_dcp (film);

	auto B = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2("reels_test9b", {B, content_factory("test/data/dcp_sub4.xml")[0]});
	B->set_reference_video(true);
	B->set_reference_audio(true);
	film2->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film2->write_metadata();
	make_and_verify_dcp (
		film2,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});
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
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto B = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto ov = new_test_film2("reels_test10_ov", {A, B});
	A->video->set_length (5 * 24);
	B->video->set_length (5 * 24);

	ov->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	make_and_verify_dcp (ov);
	ov->write_metadata ();

	/* Now try to make the VF; this used to fail */
	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	auto vf = new_test_film2("reels_test10_vf", {ov_dcp, content_factory("test/data/15s.srt")[0]});
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	ov_dcp->set_reference_video (true);
	ov_dcp->set_reference_audio (true);

	make_and_verify_dcp (
		vf,
		{
			dcp::VerificationNote::Code::EXTERNAL_ASSET,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
		});
}


/** Another reels error; ReelType::BY_VIDEO_CONTENT when the first content is not
 *  at time 0.
 */
BOOST_AUTO_TEST_CASE (reels_test11)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_test11", {A});
	film->set_video_frame_rate (24);
	A->video->set_length (240);
	A->set_video_frame_rate (24);
	A->set_position (film, DCPTime::from_seconds(1));
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	make_and_verify_dcp (film);
	BOOST_CHECK_EQUAL (A->position().get(), DCPTime::from_seconds(1).get());
	BOOST_CHECK_EQUAL (A->end(film).get(), DCPTime::from_seconds(1 + 10).get());

	auto r = film->reels ();
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
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto B = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_test12", {A, B});
	film->set_video_frame_rate (24);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	film->set_sequence (false);

	A->video->set_length (240);
	A->set_video_frame_rate (24);
	A->set_position (film, DCPTime::from_seconds(1));

	B->video->set_length (120);
	B->set_video_frame_rate (24);
	B->set_position (film, DCPTime::from_seconds(14));

	auto r = film->reels ();
	BOOST_REQUIRE_EQUAL (r.size(), 4U);
	auto i = r.begin ();

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
dump_notes (vector<dcp::VerificationNote> const & notes)
{
	for (auto i: notes) {
		std::cout << dcp::note_to_string(i) << "\n";
	}
}


/** Using less than 1 second's worth of content should not result in a reel
 *  of less than 1 second's duration.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short1)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto B = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_should_not_be_short1", {A, B});
	film->set_video_frame_rate (24);

	A->video->set_length (23);

	B->video->set_length (23);
	B->set_position (film, DCPTime::from_frames(23, 24));

	make_and_verify_dcp (film);

	vector<boost::filesystem::path> dirs = { film->dir(film->dcp_name(false)) };
	auto notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}


/** Leaving less than 1 second's gap between two pieces of content with
 *  ReelType::BY_VIDEO_CONTENT should not make a <1s reel.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short2)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto B = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_should_not_be_short2", {A, B});
	film->set_video_frame_rate (24);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	A->video->set_length (240);

	B->video->set_length (240);
	B->set_position (film, DCPTime::from_seconds(10.2));

	make_and_verify_dcp (film);

	vector<boost::filesystem::path> dirs = { film->dir(film->dcp_name(false)) };
	auto const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}


/** Setting ReelType::BY_LENGTH and using a small length value should not make
 *  <1s reels.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short3)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_should_not_be_short3", {A});
	film->set_video_frame_rate (24);
	film->set_reel_type (ReelType::BY_LENGTH);
	film->set_reel_length (1024 * 1024 * 10);

	A->video->set_length (240);

	make_and_verify_dcp (film);

	auto const notes = dcp::verify({}, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}


/** Having one piece of content less than 1s long in ReelType::BY_VIDEO_CONTENT
 *  should not make a reel less than 1s long.
 */
BOOST_AUTO_TEST_CASE (reels_should_not_be_short4)
{
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto B = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film = new_test_film2 ("reels_should_not_be_short4", {A, B});
	film->set_video_frame_rate (24);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	A->video->set_length (240);

	B->video->set_length (23);
	B->set_position (film, DCPTime::from_frames(240, 24));

	BOOST_CHECK_EQUAL (film->reels().size(), 1U);
	BOOST_CHECK (film->reels().front() == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(), dcpomatic::DCPTime::from_frames(263, 24)));

	film->write_metadata ();
	make_dcp (film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_REQUIRE (!wait_for_jobs());

	vector<boost::filesystem::path> dirs = { film->dir(film->dcp_name(false)) };
	auto const notes = dcp::verify(dirs, boost::bind(&no_op), boost::bind(&no_op), TestPaths::xsd());
	dump_notes (notes);
	BOOST_REQUIRE (notes.empty());
}


/** Create a long DCP A then insert it repeatedly into a new project, trimming it differently each time.
 *  Make a DCP B from that project which refers to A and splits into reels.  This was found to go wrong
 *  when looking at #2268.
 */
BOOST_AUTO_TEST_CASE (repeated_dcp_into_reels)
{
	/* Make a 20s DCP */
	auto A = make_shared<FFmpegContent>("test/data/flat_red.png");
	auto film1 = new_test_film2("repeated_dcp_into_reels1", { A });
	auto constexpr frame_rate = 24;
	auto constexpr length_in_seconds = 20;
	auto constexpr total_frames = frame_rate * length_in_seconds;
	film1->set_video_frame_rate(frame_rate);
	A->video->set_length(total_frames);
	make_and_verify_dcp(film1);

	/* Make a new project that includes this long DCP 4 times, each
	 * trimmed to a quarter of the original, i.e.
	 * /----------------------|----------------------|----------------------|----------------------\
	 * | 1st quarter of film1 | 2nd quarter of film1 | 3rd quarter of film1 | 4th quarter of film1 |
	 * \----------------------|----------------------|----------------------|_---------------------/
	 */

	shared_ptr<DCPContent> original_dcp[4] = {
		 make_shared<DCPContent>(film1->dir(film1->dcp_name(false))),
		 make_shared<DCPContent>(film1->dir(film1->dcp_name(false))),
		 make_shared<DCPContent>(film1->dir(film1->dcp_name(false))),
		 make_shared<DCPContent>(film1->dir(film1->dcp_name(false)))
	};

	auto film2 = new_test_film2("repeated_dcp_into_reels2", { original_dcp[0], original_dcp[1], original_dcp[2], original_dcp[3] });
	film2->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film2->set_video_frame_rate(frame_rate);
	film2->set_sequence(false);

	for (int i = 0; i < 4; ++i) {
		original_dcp[i]->set_position(film2, DCPTime::from_frames(total_frames * i / 4, frame_rate));
		original_dcp[i]->set_trim_start(ContentTime::from_frames(total_frames * i / 4, frame_rate));
		original_dcp[i]->set_trim_end  (ContentTime::from_frames(total_frames * (4 - i - 1) / 4, frame_rate));
		original_dcp[i]->set_reference_video(true);
		original_dcp[i]->set_reference_audio(true);
	}

	make_and_verify_dcp(film2, { dcp::VerificationNote::Code::EXTERNAL_ASSET });

	dcp::DCP check1(film1->dir(film1->dcp_name()));
	check1.read();
	BOOST_REQUIRE(!check1.cpls().empty());
	BOOST_REQUIRE(!check1.cpls()[0]->reels().empty());
	auto picture = check1.cpls()[0]->reels()[0]->main_picture()->asset();
	BOOST_REQUIRE(picture);
	auto sound = check1.cpls()[0]->reels()[0]->main_sound()->asset();
	BOOST_REQUIRE(sound);

	dcp::DCP check2(film2->dir(film2->dcp_name()));
	check2.read();
	BOOST_REQUIRE(!check2.cpls().empty());
	auto cpl = check2.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 4U);
	for (int i = 0; i < 4; ++i) {
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_picture()->entry_point().get_value_or(0), total_frames * i / 4);
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_picture()->duration().get_value_or(0), total_frames / 4);
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_picture()->id(), picture->id());
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_sound()->entry_point().get_value_or(0), total_frames * i / 4);
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_sound()->duration().get_value_or(0), total_frames / 4);
		BOOST_REQUIRE_EQUAL(cpl->reels()[i]->main_sound()->id(), sound->id());
	}
}

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


/** @file  test/vf_Test.cc
 *  @brief Various VF-related tests.
 *  @ingroup feature
 */


#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "lib/referenced_reel_asset.h"
#include "lib/player.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::list;
using std::string;
using std::cout;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;
using namespace dcpomatic;


/** Test the logic which decides whether a DCP can be referenced or not */
BOOST_AUTO_TEST_CASE (vf_test1)
{
	auto film = new_test_film ("vf_test1");
	film->set_interop (false);
	auto dcp = make_shared<DCPContent>("test/data/reels_test2");
	film->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Multi-reel DCP can't be referenced if we are using a single reel for the project */
	film->set_reel_type (ReelType::SINGLE);
	string why_not;
	BOOST_CHECK (!dcp->can_reference_video(film, why_not));
	BOOST_CHECK (!dcp->can_reference_audio(film, why_not));
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::OPEN_SUBTITLE, why_not));
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::CLOSED_CAPTION, why_not));

	/* Multi-reel DCP can be referenced if we are using by-video-content */
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	BOOST_CHECK (dcp->can_reference_video(film, why_not));
	BOOST_CHECK (dcp->can_reference_audio(film, why_not));
	/* (but reels_test2 has no texts to reference) */
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::OPEN_SUBTITLE, why_not));
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::CLOSED_CAPTION, why_not));

	auto other = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (other);
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_CHECK (!other->audio);

	/* Not possible if there is overlap; we only check video here as that's all test.mp4 has */
	other->set_position (film, DCPTime());
	BOOST_CHECK (!dcp->can_reference_video(film, why_not));

	/* This should not be considered an overlap */
	other->set_position (film, dcp->end(film));
	BOOST_CHECK (dcp->can_reference_video(film, why_not));
	BOOST_CHECK (dcp->can_reference_audio(film, why_not));
	/* (reels_test2 has no texts to reference) */
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::OPEN_SUBTITLE, why_not));
	BOOST_CHECK (!dcp->can_reference_text(film, TextType::CLOSED_CAPTION, why_not));
}


/** Make a OV with video and audio and a VF referencing the OV and adding subs */
BOOST_AUTO_TEST_CASE (vf_test2)
{
	/* Make the OV */
	auto ov = new_test_film ("vf_test2_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	ov->set_name ("vf_test2_ov");
	auto video = content_factory("test/data/flat_red.png").front();
	ov->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());
	video->video->set_length (24 * 5);
	auto audio = content_factory("test/data/white.wav").front();
	ov->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov);

	/* Make the VF */
	auto vf = new_test_film ("vf_test2_vf");
	vf->set_name ("vf_test2_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	vf->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);
	auto sub = content_factory("test/data/subrip4.srt").front();
	vf->examine_and_add_content (sub);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (
		vf,
		{
			dcp::VerificationNote::Code::EXTERNAL_ASSET,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION
		});

	dcp::DCP ov_c (ov->dir(ov->dcp_name()));
	ov_c.read ();
	BOOST_REQUIRE_EQUAL (ov_c.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (ov_c.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (ov_c.cpls()[0]->reels()[0]->main_picture());
	string const pic_id = ov_c.cpls()[0]->reels()[0]->main_picture()->id();
	BOOST_REQUIRE (ov_c.cpls()[0]->reels()[0]->main_sound());
	string const sound_id = ov_c.cpls()[0]->reels()[0]->main_sound()->id();
	BOOST_REQUIRE (!ov_c.cpls()[0]->reels()[0]->main_subtitle());

	dcp::DCP vf_c (vf->dir(vf->dcp_name()));
	vf_c.read ();
	BOOST_REQUIRE_EQUAL (vf_c.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (vf_c.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels()[0]->main_picture());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_picture()->id(), pic_id);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels()[0]->main_sound());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_sound()->id(), sound_id);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels()[0]->main_subtitle());
}


/** Test creation of a VF using a trimmed OV; the output should have entry point /
 *  duration altered to effect the trimming.
 */
BOOST_AUTO_TEST_CASE (vf_test3)
{
	/* Make the OV */
	auto ov = new_test_film ("vf_test3_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	ov->set_name ("vf_test3_ov");
	auto video = content_factory("test/data/flat_red.png").front();
	ov->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());
	video->video->set_length (24 * 5);
	auto audio = content_factory("test/data/white.wav").front();
	ov->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov);

	/* Make the VF */
	auto vf = new_test_film ("vf_test3_vf");
	vf->set_name ("vf_test3_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	BOOST_REQUIRE (dcp);
	dcp->set_trim_start (ContentTime::from_seconds (1));
	dcp->set_trim_end (ContentTime::from_seconds (1));
	vf->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);
	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	dcp::DCP vf_c (vf->dir(vf->dcp_name()));
	vf_c.read ();
	BOOST_REQUIRE_EQUAL (vf_c.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (vf_c.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels()[0]->main_picture());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_picture()->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_picture()->actual_duration(), 72);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels()[0]->main_sound());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_sound()->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels()[0]->main_sound()->actual_duration(), 72);
}


/** Make a OV with video and audio and a VF referencing the OV and adding some more video */
BOOST_AUTO_TEST_CASE (vf_test4)
{
	/* Make the OV */
	auto ov = new_test_film ("vf_test4_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	ov->set_name ("vf_test4_ov");
	auto video = content_factory("test/data/flat_red.png").front();
	ov->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());
	video->video->set_length (24 * 5);
	auto audio = content_factory("test/data/white.wav").front();
	ov->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov);

	/* Make the VF */
	auto vf = new_test_film ("vf_test4_vf");
	vf->set_name ("vf_test4_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	vf->set_sequence (false);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	BOOST_REQUIRE (dcp);
	vf->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	dcp->set_position(vf, DCPTime::from_seconds(10));
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);
	auto more_video = content_factory("test/data/flat_red.png").front();
	vf->examine_and_add_content (more_video);
	BOOST_REQUIRE (!wait_for_jobs());
	more_video->set_position (vf, DCPTime());
	vf->write_metadata ();
	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	dcp::DCP ov_c (ov->dir(ov->dcp_name()));
	ov_c.read ();
	BOOST_REQUIRE_EQUAL (ov_c.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (ov_c.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (ov_c.cpls()[0]->reels()[0]->main_picture());
	string const pic_id = ov_c.cpls()[0]->reels()[0]->main_picture()->id();
	BOOST_REQUIRE (ov_c.cpls()[0]->reels()[0]->main_sound());
	string const sound_id = ov_c.cpls()[0]->reels()[0]->main_sound()->id();
	BOOST_REQUIRE (!ov_c.cpls()[0]->reels()[0]->main_subtitle());

	dcp::DCP vf_c (vf->dir (vf->dcp_name ()));
	vf_c.read ();
	BOOST_REQUIRE_EQUAL (vf_c.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (vf_c.cpls()[0]->reels().size(), 2U);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels().back()->main_picture());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels().back()->main_picture()->id(), pic_id);
	BOOST_REQUIRE (vf_c.cpls()[0]->reels().back()->main_sound());
	BOOST_CHECK_EQUAL (vf_c.cpls()[0]->reels().back()->main_sound()->id(), sound_id);
}


/** Test bug #1495 */
BOOST_AUTO_TEST_CASE (vf_test5)
{
	/* Make the OV */
	auto ov = new_test_film ("vf_test5_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	ov->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	for (int i = 0; i < 3; ++i) {
		auto video = content_factory("test/data/flat_red.png").front();
		ov->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (24 * 10);
	}

	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov);

	/* Make the VF */
	auto vf = new_test_film ("vf_test5_vf");
	vf->set_name ("vf_test5_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	vf->set_sequence (false);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	BOOST_REQUIRE (dcp);
	vf->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);
	dcp->set_trim_end (ContentTime::from_seconds(15));
	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	/* Check that the selected reel assets are right */
	auto player = make_shared<Player>(vf, Image::Alignment::COMPACT);
	auto a = player->get_reel_assets();
	BOOST_REQUIRE_EQUAL (a.size(), 4U);
	auto i = a.begin();
	BOOST_CHECK (i->period == DCPTimePeriod(DCPTime(0), DCPTime(960000)));
	++i;
	BOOST_CHECK (i->period == DCPTimePeriod(DCPTime(0), DCPTime(960000)));
	++i;
	BOOST_CHECK (i->period == DCPTimePeriod(DCPTime(960000), DCPTime(1440000)));
	++i;
	BOOST_CHECK (i->period == DCPTimePeriod(DCPTime(960000), DCPTime(1440000)));
	++i;
}


/** Test bug #1528 */
BOOST_AUTO_TEST_CASE (vf_test6)
{
	/* Make the OV */
	auto ov = new_test_film ("vf_test6_ov");
	ov->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	ov->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	auto video = content_factory("test/data/flat_red.png").front();
	ov->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());
	video->video->set_length (24 * 10);
	make_and_verify_dcp (ov);

	/* Make the VF */
	auto vf = new_test_film ("vf_test6_vf");
	vf->set_name ("vf_test6_vf");
	vf->set_dcp_content_type (DCPContentType::from_isdcf_name("TST"));
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	vf->set_sequence (false);
	auto dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	vf->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());
	dcp->set_reference_video (true);
	dcp->set_reference_audio (true);

	auto sub = content_factory("test/data/15s.srt").front();
	vf->examine_and_add_content (sub);
	BOOST_REQUIRE (!wait_for_jobs());

	make_and_verify_dcp (
		vf,
		{
			dcp::VerificationNote::Code::EXTERNAL_ASSET,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});
}


/** Test bug #1643 (the second part; referring fails if there are gaps) */
BOOST_AUTO_TEST_CASE (vf_test7)
{
	/* First OV */
	auto ov1 = new_test_film2 ("vf_test7_ov1", {content_factory("test/data/flat_red.png").front()});
	ov1->set_video_frame_rate (24);
	make_and_verify_dcp (ov1);

	/* Second OV */
	auto ov2 = new_test_film2 ("vf_test7_ov2", {content_factory("test/data/flat_red.png").front()});
	ov2->set_video_frame_rate (24);
	make_and_verify_dcp (ov2);

	/* VF */
	auto ov1_dcp = make_shared<DCPContent>(ov1->dir(ov1->dcp_name()));
	auto ov2_dcp = make_shared<DCPContent>(ov2->dir(ov2->dcp_name()));
	auto vf = new_test_film2 ("vf_test7_vf", {ov1_dcp, ov2_dcp});
	vf->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	ov1_dcp->set_reference_video (true);
	ov2_dcp->set_reference_video (true);
	ov1_dcp->set_position (vf, DCPTime::from_seconds(1));
	ov2_dcp->set_position (vf, DCPTime::from_seconds(20));
	vf->write_metadata ();
	make_and_verify_dcp (vf);
}

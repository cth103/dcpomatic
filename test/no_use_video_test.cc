/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/test_no_use_video.cc
 *  @brief Test some cases where the video parts of inputs are ignored, to
 *  check that the right DCPs are made.
 *  @ingroup completedcp
 */


#include "lib/audio_content.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;


/** Overlay two video-only bits of content, don't use the video on one and
 *  make sure the other one is in the DCP.
 */
BOOST_AUTO_TEST_CASE (no_use_video_test1)
{
	auto film = new_test_film2 ("no_use_video_test1");
	auto A = content_factory("test/data/flat_red.png").front();
	auto B = content_factory("test/data/flat_green.png").front();
	film->examine_and_add_content (A);
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());

	A->set_position (film, dcpomatic::DCPTime());
	B->set_position (film, dcpomatic::DCPTime());
	A->video->set_use (false);

	make_and_verify_dcp (film);

	check_dcp ("test/data/no_use_video_test1", film);
}


/** Overlay two muxed sources and disable the video on one */
BOOST_AUTO_TEST_CASE (no_use_video_test2)
{
	auto film = new_test_film2 ("no_use_video_test2");
	auto A = content_factory (TestPaths::private_data() / "dolby_aurora.vob").front();
	auto B = content_factory (TestPaths::private_data() / "big_buck_bunny_trailer_480p.mov").front();
	film->examine_and_add_content (A);
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());

	A->set_position (film, dcpomatic::DCPTime());
	B->set_position (film, dcpomatic::DCPTime());
	A->video->set_use (false);

	make_and_verify_dcp (film);

	check_dcp (TestPaths::private_data() / "no_use_video_test2", film);
}


/** Make two DCPs and make a VF with the audio from one and the video from another */
BOOST_AUTO_TEST_CASE (no_use_video_test3)
{
	auto ov_a = new_test_film2 ("no_use_video_test3_ov_a");
	auto ov_a_pic = content_factory("test/data/flat_red.png").front();
	BOOST_REQUIRE (ov_a_pic);
	auto ov_a_snd = content_factory("test/data/sine_16_48_220_10.wav").front();
	BOOST_REQUIRE (ov_a_snd);
	ov_a->examine_and_add_content (ov_a_pic);
	ov_a->examine_and_add_content (ov_a_snd);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov_a);

	auto ov_b = new_test_film2("no_use_video_test3_ov_b");
	auto ov_b_pic = content_factory("test/data/flat_green.png").front();
	BOOST_REQUIRE (ov_b_pic);
	auto ov_b_snd = content_factory("test/data/sine_16_48_880_10.wav").front();
	BOOST_REQUIRE (ov_b_snd);
	ov_b->examine_and_add_content (ov_b_pic);
	ov_b->examine_and_add_content (ov_b_snd);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (ov_b);

	auto vf = new_test_film2 ("no_use_video_test3_vf");
	auto A = make_shared<DCPContent>(ov_a->dir(ov_a->dcp_name()));
	auto B = make_shared<DCPContent>(ov_b->dir(ov_b->dcp_name()));
	vf->examine_and_add_content (A);
	vf->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());

	A->set_position (vf, dcpomatic::DCPTime());
	A->video->set_use (false);
	B->set_position (vf, dcpomatic::DCPTime());
	AudioMapping mapping (16, 16);
	mapping.make_zero ();
	B->audio->set_mapping(mapping);

	A->set_reference_audio (true);
	B->set_reference_video (true);

	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	dcp::DCP ov_a_check (ov_a->dir(ov_a->dcp_name()));
	ov_a_check.read ();
	BOOST_REQUIRE_EQUAL (ov_a_check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (ov_a_check.cpls().front()->reels().size(), 1U);
	auto ov_a_reel (ov_a_check.cpls().front()->reels().front());

	dcp::DCP ov_b_check (ov_b->dir(ov_b->dcp_name()));
	ov_b_check.read ();
	BOOST_REQUIRE_EQUAL (ov_b_check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (ov_b_check.cpls().front()->reels().size(), 1U);
	auto ov_b_reel (ov_b_check.cpls().front()->reels().front());

	dcp::DCP vf_check (vf->dir(vf->dcp_name()));
	vf_check.read ();
	BOOST_REQUIRE_EQUAL (vf_check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (vf_check.cpls().front()->reels().size(), 1U);
	auto vf_reel (vf_check.cpls().front()->reels().front());

	BOOST_CHECK_EQUAL (vf_reel->main_picture()->asset_ref().id(), ov_b_reel->main_picture()->asset_ref().id());
	BOOST_CHECK_EQUAL (vf_reel->main_sound()->asset_ref().id(), ov_a_reel->main_sound()->asset_ref().id());
}



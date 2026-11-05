/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  test/make_dcp_with_reused_assets_test.cc
 *  @brief Make some DCPs from other DCPs, reusing assets (or not).
 *  @ingroup completedcp
 */


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <boost/test/unit_test.hpp>
#include <functional>


using std::shared_ptr;


void check_video_passed_through(shared_ptr<const Film> film)
{
	check_file(
		"test/data/scaling_test_185_185/j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf",
		film->dir(film->dcp_name()) / "j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf"
	);
}


void check_audio_passed_through(shared_ptr<const Film> film)
{
	check_file(
		"test/data/scaling_test_185_185/pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf",
		film->dir(film->dcp_name()) / "pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf"
	);
}


void check_video_not_passed_through(shared_ptr<const Film> film)
{
	BOOST_CHECK(!boost::filesystem::exists(film->dir(film->dcp_name()) / "j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf"));
}


void check_audio_not_passed_through(shared_ptr<const Film> film)
{
	BOOST_CHECK(!boost::filesystem::exists(film->dir(film->dcp_name()) / "pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf"));
}


/** Basic pass-through of picture and sound */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test1)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test1", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_passed_through(film);
	check_audio_passed_through(film);
}


/** No pass-through with the wrong standard */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test2)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test2", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);
	film->set_interop(true);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	check_video_not_passed_through(film);
	check_audio_not_passed_through(film);
}



/** No audio pass-through with the wrong channel count */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test3)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test3", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(6);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_passed_through(film);
	check_audio_not_passed_through(film);
}


/** Video pass-through with non-burnt subtitles */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test4)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reuse_dcp_asset_test4", { input, subs });
	subs->text[0]->set_burn(false);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE, dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME });

	check_video_passed_through(film);
	check_audio_passed_through(film);
}


/** Video not passed through with burnt subtitles */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test5)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reuse_dcp_asset_test4", { input, subs });
	subs->text[0]->set_burn(true);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_not_passed_through(film);
	check_audio_passed_through(film);
}


/** @return the ID of the Atmos asset in a reel of the single CPL of a DCP, or an empty string if that reel has no Atmos */
static std::string atmos_asset_id(shared_ptr<const Film> film, int reel_index = 0)
{
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto cpls = dcp.cpls();
	BOOST_REQUIRE_EQUAL(cpls.size(), 1U);
	auto reels = cpls[0]->reels();
	BOOST_REQUIRE(static_cast<int>(reels.size()) > reel_index);
	auto atmos = reels[reel_index]->atmos();
	return atmos ? atmos->asset_ref().id() : std::string();
}


static shared_ptr<Film> make_atmos_dcp(std::string name)
{
	auto film = new_test_film(name, content_factory("test/data/atmos_0.mxf"));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return film;
}


/** Make a film that has the DCP of @ref input as its content, and make its DCP */
static shared_ptr<Film> make_atmos_dcp_from_dcp(
	std::string name,
	shared_ptr<const Film> input,
	std::function<void (shared_ptr<Film>)> setup = {},
	std::vector<dcp::VerificationNote::Code> ignore = {}
	)
{
	ignore.push_back(dcp::VerificationNote::Code::MISSING_CPL_METADATA);
	auto content = std::make_shared<DCPContent>(input->dir(input->dcp_name()));
	auto film = new_test_film(name, { content });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_interop(false);
	if (setup) {
		setup(film);
	}
	make_and_verify_dcp(film, ignore);
	return film;
}


/** Atmos pass-through: the Atmos of the input DCP is the Atmos of the new one */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test1)
{
	auto input = make_atmos_dcp("reuse_dcp_asset_atmos_test1_input");
	auto film = make_atmos_dcp_from_dcp("reuse_dcp_asset_atmos_test1", input);

	BOOST_REQUIRE(!atmos_asset_id(input).empty());
	BOOST_CHECK_EQUAL(atmos_asset_id(film), atmos_asset_id(input));
}


/** No Atmos pass-through when the film encrypts its sound */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test2)
{
	auto input = make_atmos_dcp("reuse_dcp_asset_atmos_test2_input");
	auto film = make_atmos_dcp_from_dcp("reuse_dcp_asset_atmos_test2", input, [](shared_ptr<Film> f) {
		f->set_encrypt_picture(true);
		f->set_encrypt_sound(true);
		f->set_encrypt_text(true);
	}, { dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED });

	BOOST_REQUIRE(!atmos_asset_id(film).empty());
	BOOST_CHECK(atmos_asset_id(film) != atmos_asset_id(input));
}


/** No Atmos pass-through when other Atmos content is over the DCP's Atmos */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test3)
{
	auto input = make_atmos_dcp("reuse_dcp_asset_atmos_test3_input");

	auto content = std::make_shared<DCPContent>(input->dir(input->dcp_name()));
	auto other = content_factory("test/data/atmos_1.mxf")[0];
	auto film = new_test_film("reuse_dcp_asset_atmos_test3", { content, other });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_interop(false);
	other->set_position(film, dcpomatic::DCPTime());
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK(atmos_asset_id(film) != atmos_asset_id(input));
}


/** Atmos pass-through when the sound cannot be passed through because other sound is over it */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test4)
{
	auto input = make_atmos_dcp("reuse_dcp_asset_atmos_test4_input");

	auto content = std::make_shared<DCPContent>(input->dir(input->dcp_name()));
	auto sound = content_factory("test/data/white.wav")[0];
	auto film = new_test_film("reuse_dcp_asset_atmos_test4", { content, sound });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_interop(false);
	sound->set_position(film, dcpomatic::DCPTime());
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK_EQUAL(atmos_asset_id(film), atmos_asset_id(input));
}


/** Atmos pass-through in the one reel of three that has Atmos */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test5)
{
	std::vector<shared_ptr<Content>> all;
	std::vector<shared_ptr<Content>> picture;
	for (int i = 0; i < 3; ++i) {
		picture.push_back(content_factory("test/data/flat_red.png")[0]);
		all.push_back(picture.back());
	}
	auto atmos = content_factory("test/data/atmos_0.mxf")[0];
	all.push_back(atmos);

	auto input = new_test_film("reuse_dcp_asset_atmos_test5_input", all);
	for (int i = 0; i < 3; ++i) {
		picture[i]->set_position(input, dcpomatic::DCPTime::from_seconds(i * 10));
	}
	atmos->set_position(input, dcpomatic::DCPTime());
	make_and_verify_dcp(input, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	auto film = make_atmos_dcp_from_dcp("reuse_dcp_asset_atmos_test5", input, [](shared_ptr<Film> f) {
		f->set_reel_type(ReelType::CUSTOM);
		f->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(10), dcpomatic::DCPTime::from_seconds(20) });
	});

	BOOST_REQUIRE(!atmos_asset_id(input, 0).empty());
	BOOST_CHECK_EQUAL(atmos_asset_id(film, 0), atmos_asset_id(input, 0));
	BOOST_CHECK(atmos_asset_id(film, 1).empty());
	BOOST_CHECK(atmos_asset_id(film, 2).empty());
}


/** No Atmos pass-through when the DCP is trimmed: the whole asset would be out of sync with the reel.
 *  A burnt-in subtitle and other sound stop the picture and the sound from being passed through, as
 *  those two fail with an error when they are trimmed; the Atmos has no such check.
 */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_atmos_test6)
{
	auto input = make_atmos_dcp("reuse_dcp_asset_atmos_test6_input");

	auto content = std::make_shared<DCPContent>(input->dir(input->dcp_name()));
	auto subs = content_factory("test/data/15s.srt")[0];
	auto sound = content_factory("test/data/white.wav")[0];
	auto film = new_test_film("reuse_dcp_asset_atmos_test6", { content, subs, sound });
	subs->text[0]->set_burn(true);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_interop(false);
	content->set_trim_start(dcpomatic::ContentTime::from_seconds(1));
	sound->set_position(film, dcpomatic::DCPTime());
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE, dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME });

	BOOST_REQUIRE(!atmos_asset_id(film).empty());
	BOOST_CHECK(atmos_asset_id(film) != atmos_asset_id(input));
}

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
#include <boost/test/unit_test.hpp>


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

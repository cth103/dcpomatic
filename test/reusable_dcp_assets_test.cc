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


/** @file  test/reusable_dcp_assets_test.cc
 *  @brief Test Film::reusable_dcp_assets()
 *  @ingroup selfcontained
 */


#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::vector;


static
void
check_for_asset(vector<DCPAsset> const& reusable, boost::filesystem::path file)
{
	BOOST_CHECK_MESSAGE(std::find_if(reusable.begin(), reusable.end(), [file](DCPAsset const& asset) {
		return asset.file() == boost::filesystem::canonical(file);
	}) != reusable.end(), file << " not found");
}


BOOST_AUTO_TEST_CASE(reusable_assets_from_single_dcp_project)
{
	auto dcp = make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reusable_assets_from_single_dcp_project", { dcp });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);

	auto reusable = film->reusable_dcp_assets();

	BOOST_CHECK_EQUAL(reusable.size(), 2U);
	check_for_asset(reusable, "test/data/scaling_test_185_185/j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf");
	check_for_asset(reusable, "test/data/scaling_test_185_185/pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf");
}


BOOST_AUTO_TEST_CASE(no_reuse_wrong_standard)
{
	auto dcp = make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reusable_assets_from_single_dcp_project", { dcp });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);
	film->set_interop(true);

	BOOST_CHECK(film->reusable_dcp_assets().empty());
}


BOOST_AUTO_TEST_CASE(no_reuse_video_with_burnt_subs)
{
	auto dcp = make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reusable_assets_from_single_dcp_project", { dcp, subs });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	subs->text[0]->set_use(true);
	subs->text[0]->set_burn(true);

	auto reusable = film->reusable_dcp_assets();

	BOOST_CHECK_EQUAL(reusable.size(), 1U);
	check_for_asset(reusable, "test/data/scaling_test_185_185/pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf");
}


BOOST_AUTO_TEST_CASE(reuse_video_with_unburnt_subs)
{
	auto dcp = make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reusable_assets_from_single_dcp_project", { dcp, subs });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	subs->text[0]->set_use(true);

	BOOST_CHECK_EQUAL(film->reusable_dcp_assets().size(), 2U);
}


BOOST_AUTO_TEST_CASE(no_reuse_audio_with_channel_remap)
{
	auto dcp = make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reusable_assets_from_single_dcp_project", { dcp });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	auto mapping = dcp->audio->mapping();
	mapping.set(0, 0, 0);
	dcp->audio->set_mapping(mapping);

	auto reusable = film->reusable_dcp_assets();

	BOOST_CHECK_EQUAL(reusable.size(), 1U);
	check_for_asset(reusable, "test/data/scaling_test_185_185/j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf");
}



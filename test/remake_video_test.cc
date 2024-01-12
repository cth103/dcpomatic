/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/colour_conversion.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE(remake_video_after_yub_rgb_matrix_changed)
{
	auto content = content_factory("test/data/rgb_grey_testcard.mp4")[0];
	auto film = new_test_film2("remake_video_after_yub_rgb_matrix_changed", { content });

	auto conversion = content->video->colour_conversion();
	BOOST_REQUIRE(static_cast<bool>(conversion));
	conversion->set_yuv_to_rgb(dcp::YUVToRGB::REC709);
	content->video->set_colour_conversion(*conversion);

	auto calculate_picture_hashes = [](shared_ptr<Film> film) {
		/* >1 CPLs in the DCP raises an error in ClairMeta */
		make_and_verify_dcp(film, {}, true, false);
		dcp::DCP dcp(film->dir(film->dcp_name()));
		dcp.read();
		BOOST_REQUIRE(!dcp.cpls().empty());
		auto cpl = dcp.cpls()[0];
		BOOST_REQUIRE(!cpl->reels().empty());
		auto reel = cpl->reels()[0];
		BOOST_REQUIRE(reel->main_picture());
		auto mono = dynamic_pointer_cast<dcp::MonoPictureAsset>(reel->main_picture()->asset());
		BOOST_REQUIRE(mono);
		auto reader = mono->start_read();

		vector<string> hashes;
		for (auto i = 0; i < reel->main_picture()->intrinsic_duration(); ++i) {
			auto frame = reader->get_frame(i);
			hashes.push_back(dcp::make_digest(dcp::ArrayData(frame->data(), frame->size())));
		}
		return hashes;
	};

	auto before = calculate_picture_hashes(film);
	conversion->set_yuv_to_rgb(dcp::YUVToRGB::REC601);
	content->video->set_colour_conversion(*conversion);
	auto after = calculate_picture_hashes(film);

	BOOST_CHECK(before != after);
}


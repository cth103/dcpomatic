/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/mono_j2k_picture_asset_reader.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::string;
using std::list;


BOOST_AUTO_TEST_CASE(image_content_fade_in_test)
{
	auto film = new_test_film("image_content_fade_in_test");
	auto content = content_factory("test/data/flat_red.png")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs());

	content->video->set_fade_in (1);
	make_and_verify_dcp (film);

	/* This test is concerned with the image, so we'll ignore any
	 * differences in sound between the DCP and the reference to avoid test
	 * failures for unrelated reasons.
	 */
	check_dcp("test/data/image_content_fade_in_test", film->dir(film->dcp_name()), true);
}


BOOST_AUTO_TEST_CASE(image_content_fade_out_test)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("image_content_fade_out_test", {content});
	content->video->set_fade_out(12);
	make_and_verify_dcp(film);

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto picture = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(dcp.cpls()[0]->reels()[0]->main_picture()->asset());
	auto reader = picture->start_read();
	auto frame = reader->get_frame(239)->xyz_image();

	auto const width = frame->size().width;
	auto const height = frame->size().height;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			for (int c = 0; c < 2; ++c) {
				BOOST_REQUIRE(frame->data(c)[y * width + x] <= 4);
			}
		}
	}
}

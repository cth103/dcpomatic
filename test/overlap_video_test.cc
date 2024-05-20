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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcpomatic_time.h"
#include "lib/film.h"
#include "lib/piece.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/mono_j2k_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::vector;


BOOST_AUTO_TEST_CASE (overlap_video_test1)
{
	auto A = content_factory("test/data/flat_red.png")[0];
	auto B = content_factory("test/data/flat_green.png")[0];
	auto C = content_factory("test/data/flat_blue.png")[0];
	auto film = new_test_film("overlap_video_test1", { A, B, C });
	film->set_sequence (false);

	auto const fps = 24;

	// 01234
	// AAAAA
	//  B
	//    C

	A->video->set_length(5 * fps);
	B->video->set_length(1 * fps);
	C->video->set_length(1 * fps);

	B->set_position(film, dcpomatic::DCPTime::from_seconds(1));
	C->set_position(film, dcpomatic::DCPTime::from_seconds(3));

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
	auto pieces = player->_pieces;
	BOOST_REQUIRE_EQUAL (pieces.size(), 3U);
	BOOST_CHECK_EQUAL(pieces[0]->content, A);
	BOOST_CHECK_EQUAL(pieces[1]->content, B);
	BOOST_CHECK_EQUAL(pieces[2]->content, C);
	BOOST_CHECK_EQUAL(pieces[0]->ignore_video.size(), 2U);
	BOOST_CHECK(pieces[0]->ignore_video[0] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime::from_seconds(1), dcpomatic::DCPTime::from_seconds(1) + B->length_after_trim(film)));
	BOOST_CHECK(pieces[0]->ignore_video[1] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime::from_seconds(3), dcpomatic::DCPTime::from_seconds(3) + C->length_after_trim(film)));

	BOOST_CHECK (player->_black.done());

	make_and_verify_dcp (film);

	dcp::DCP back (film->dir(film->dcp_name()));
	back.read ();
	BOOST_REQUIRE_EQUAL (back.cpls().size(), 1U);
	auto cpl = back.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	BOOST_REQUIRE (reel->main_picture());
	auto mono_picture = dynamic_pointer_cast<dcp::ReelMonoPictureAsset>(reel->main_picture());
	BOOST_REQUIRE (mono_picture);
	auto asset = mono_picture->mono_j2k_asset();
	BOOST_REQUIRE (asset);
	BOOST_CHECK_EQUAL (asset->intrinsic_duration(), fps * 5);
	auto reader = asset->start_read ();

	auto close = [](shared_ptr<const dcp::OpenJPEGImage> image, vector<int> rgb) {
		for (int component = 0; component < 3; ++component) {
			BOOST_REQUIRE(std::abs(image->data(component)[0] - rgb[component]) < 2);
		}
	};

	vector<int> const red = { 2808, 2176, 865 };
	vector<int> const blue = { 2657, 3470, 1742 };
	vector<int> const green = { 2044, 1437, 3871 };

	for (int i = 0; i < 5 * fps; ++i) {
		auto frame = reader->get_frame (i);
		auto image = dcp::decompress_j2k(*frame.get(), 0);
		if (i < fps) {
			close(image, red);
		} else if (i < 2 * fps) {
			close(image, blue);
		} else if (i < 3 * fps) {
			close(image, red);
		} else if (i < 4 * fps) {
			close(image, green);
		} else if (i < 5 * fps) {
			close(image, red);
		}
	}
}


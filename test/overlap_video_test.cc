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
#include "lib/player.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;


BOOST_AUTO_TEST_CASE (overlap_video_test1)
{
	auto film = new_test_film2 ("overlap_video_test1");
	film->set_sequence (false);
	auto A = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (A);
	auto B = content_factory("test/data/flat_green.png").front();
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs());

	A->video->set_length (72);
	B->video->set_length (24);
	B->set_position (film, dcpomatic::DCPTime::from_seconds(1));

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
	auto pieces = player->_pieces;
	BOOST_REQUIRE_EQUAL (pieces.size(), 2U);
	BOOST_CHECK_EQUAL (pieces.front()->content, A);
	BOOST_CHECK_EQUAL (pieces.back()->content, B);
	BOOST_CHECK (pieces.front()->ignore_video);
	BOOST_CHECK (pieces.front()->ignore_video.get() == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime::from_seconds(1), dcpomatic::DCPTime::from_seconds(1) + B->length_after_trim(film)));

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
	auto asset = mono_picture->mono_asset();
	BOOST_REQUIRE (asset);
	BOOST_CHECK_EQUAL (asset->intrinsic_duration(), 72);
	auto reader = asset->start_read ();

	auto close = [](int a, int b, int d) {
		BOOST_CHECK (std::abs(a - b) < d);
	};

	for (int i = 0; i < 24; ++i) {
		auto frame = reader->get_frame (i);
		auto image = dcp::decompress_j2k(*frame.get(), 0);
		close (image->data(0)[0], 2808, 2);
		close (image->data(1)[0], 2176, 2);
		close (image->data(2)[0], 865, 2);
	}

	for (int i = 24; i < 48; ++i) {
		auto frame = reader->get_frame (i);
		auto image = dcp::decompress_j2k(*frame.get(), 0);
		close (image->data(0)[0], 2657, 2);
		close (image->data(1)[0], 3470, 2);
		close (image->data(2)[0], 1742, 2);
	}

	for (int i = 48; i < 72; ++i) {
		auto frame = reader->get_frame (i);
		auto image = dcp::decompress_j2k(*frame.get(), 0);
		close (image->data(0)[0], 2808, 2);
		close (image->data(1)[0], 2176, 2);
		close (image->data(2)[0], 865, 2);
	}
}


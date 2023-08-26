/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/required_disk_space_test.cc
 *  @brief Check Film::required_disk_space
 *  @ingroup selfcontained
 */


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;


void check_within_n (int64_t a, int64_t b, int64_t n)
{
	BOOST_CHECK_MESSAGE (abs(a - b) <= n, "Estimated " << a << " differs from reference " << b << " by more than " << n);
}


BOOST_AUTO_TEST_CASE (required_disk_space_test)
{
	auto film = new_test_film ("required_disk_space_test");
	film->set_j2k_bandwidth (100000000);
	film->set_audio_channels (6);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	auto content_a = content_factory("test/data/flat_blue.png")[0];
	BOOST_REQUIRE (content_a);
	film->examine_and_add_content (content_a);
	auto content_b = make_shared<DCPContent>("test/data/burnt_subtitle_test_dcp");
	film->examine_and_add_content (content_b);
	BOOST_REQUIRE (!wait_for_jobs());
	film->write_metadata ();

	check_within_n (
		film->required_disk_space(),
		288LL * (100000000 / 8) / 24 +  // video
		288LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);

	content_b->set_reference_video (true);

	check_within_n (
		film->required_disk_space(),
		240LL * (100000000 / 8) / 24 +  // video
		288LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);

	content_b->set_reference_audio (true);

	check_within_n (
		film->required_disk_space(),
		240LL * (100000000 / 8) / 24 +  // video
		240LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);
}

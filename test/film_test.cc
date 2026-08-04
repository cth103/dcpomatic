/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(film_contains_atmos_content_test)
{
	auto atmos = content_factory("test/data/atmos_0.mxf")[0];
	auto image = content_factory("test/data/flat_red.png")[0];
	auto sound = content_factory("test/data/white.wav")[0];

	auto film1 = new_test_film("film_contains_atmos_content_test1", { atmos, image, sound });
	BOOST_CHECK(film1->contains_atmos_content());

	auto film2 = new_test_film("film_contains_atmos_content_test2", { sound, atmos, image });
	BOOST_CHECK(film2->contains_atmos_content());

	auto film3 = new_test_film("film_contains_atmos_content_test3", { image, sound, atmos });
	BOOST_CHECK(film3->contains_atmos_content());

	auto film4 = new_test_film("film_contains_atmos_content_test4", { image, sound });
	BOOST_CHECK(!film4->contains_atmos_content());
}


BOOST_AUTO_TEST_CASE(film_possible_reel_types_test1)
{
	auto film = new_test_film("film_possible_reel_types_test1");
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 4U);

	film->examine_and_add_content(content_factory("test/data/flat_red.png"));
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 4U);

	auto dcp = make_shared<DCPContent>("test/data/reels_test2");
	film->examine_and_add_content({dcp});
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 4U);

	/* If we don't do this the set_reference_video will be overridden by the Film's
	 * check_settings_consistency() stuff.
	 */
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	dcp->set_reference_video(true);
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 3U);
}


BOOST_AUTO_TEST_CASE(film_possible_reel_types_test2)
{
	auto film = new_test_film("film_possible_reel_types_test2");

	auto dcp = make_shared<DCPContent>("test/data/dcp_digest_test_dcp");
	film->examine_and_add_content({dcp});
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 4U);

	dcp->set_reference_video(true);
	BOOST_CHECK_EQUAL(film->possible_reel_types().size(), 4U);
}


BOOST_AUTO_TEST_CASE(film_copy_remembered_assets_test)
{
	dcp::filesystem::remove_all("build/test/film_copy_remembered_assets_test2");

	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("film_copy_remembered_assets_test", { content });
	make_and_verify_dcp(film);

	auto copy = make_shared<Film>(boost::filesystem::path("build/test/film_copy_remembered_assets_test2"));
	copy->copy_from(film, [](float) {});

	auto remembered = copy->read_remembered_assets();
	BOOST_REQUIRE_EQUAL(remembered.size(), 1U);
	auto path = find_asset(remembered, *copy->directory(), dcpomatic::DCPTimePeriod({}, dcpomatic::DCPTime::from_seconds(10)), film->video_identifier());
	BOOST_CHECK(path.has_value());

	for (auto path: dcp::filesystem::directory_iterator(film->dir("info"))) {
		check_file(path.path(), copy->dir("info") / path.path().filename());
	}
}


BOOST_AUTO_TEST_CASE(film_reels_test)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film_with_reels = new_test_film("film_reels_test1", { content });
	content->video->set_length(3 * 60 * 24);
	film_with_reels->set_video_bit_rate(VideoEncoding::JPEG2000, 200000000LL);
	film_with_reels->set_reel_type(ReelType::BY_LENGTH);
	film_with_reels->set_reel_length(2000000000LL);
	make_and_verify_dcp(film_with_reels);

	BOOST_REQUIRE_EQUAL(film_with_reels->reels().size(), 3U);
	BOOST_CHECK(film_with_reels->reels()[0] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(0), dcpomatic::DCPTime(7680000)));
	BOOST_CHECK(film_with_reels->reels()[1] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(7680000), dcpomatic::DCPTime(15360000)));
	BOOST_CHECK(film_with_reels->reels()[2] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(15360000), dcpomatic::DCPTime(17280000)));

	auto dcp_with_reels = make_shared<DCPContent>(film_with_reels->dir(film_with_reels->dcp_name()));
	auto test_film = new_test_film("film_reels_test2", { dcp_with_reels });
	test_film->set_video_bit_rate(VideoEncoding::JPEG2000, 200000000LL);
	test_film->set_reel_type(ReelType::BY_LENGTH);
	test_film->set_reel_length(1500000000LL);

	/* Suitable for the requested reel length, OV is not referred yet */
	BOOST_REQUIRE_EQUAL(test_film->reels().size(), 3U);
	BOOST_CHECK(test_film->reels()[0] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(0), dcpomatic::DCPTime(5760000)));
	BOOST_CHECK(test_film->reels()[1] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(5760000), dcpomatic::DCPTime(11520000)));
	BOOST_CHECK(test_film->reels()[2] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(11520000), dcpomatic::DCPTime(17280000)));

	dcp_with_reels->set_reference_video(true);

	/* Now adds in the boundaries from the OV */
	BOOST_REQUIRE_EQUAL(test_film->reels().size(), 5U);
	// First reel is what we wanted
	BOOST_CHECK(test_film->reels()[0] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(0), dcpomatic::DCPTime(5760000)));
	// Second has to end at 7680000 to get the OV's first boundary in
	BOOST_CHECK(test_film->reels()[1] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(5760000), dcpomatic::DCPTime(7680000)));
	// Third can run for our desired length (768 + 576 = 1344)
	BOOST_CHECK(test_film->reels()[2] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(7680000), dcpomatic::DCPTime(13440000)));
	// Fourth has to finish on the OV's second boundary
	BOOST_CHECK(test_film->reels()[3] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(13440000), dcpomatic::DCPTime(15360000)));
	// Finish off
	BOOST_CHECK(test_film->reels()[4] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(15360000), dcpomatic::DCPTime(17280000)));

	test_film->set_reel_type(ReelType::CUSTOM);
	test_film->set_custom_reel_boundaries({dcpomatic::DCPTime(6000000), dcpomatic::DCPTime(16000000)});

	/* Now adds in the custom boundaries */
	BOOST_REQUIRE_EQUAL(test_film->reels().size(), 5U);
	BOOST_CHECK(test_film->reels()[0] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(0), dcpomatic::DCPTime(6000000)));
	BOOST_CHECK(test_film->reels()[1] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(6000000), dcpomatic::DCPTime(7680000)));
	BOOST_CHECK(test_film->reels()[2] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(7680000), dcpomatic::DCPTime(15360000)));
	BOOST_CHECK(test_film->reels()[3] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(15360000), dcpomatic::DCPTime(16000000)));
	BOOST_CHECK(test_film->reels()[4] == dcpomatic::DCPTimePeriod(dcpomatic::DCPTime(16000000), dcpomatic::DCPTime(17280000)));
}


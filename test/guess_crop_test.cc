/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/guess_crop.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <vector>


using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (guess_crop_image_test1)
{
	auto content = content_factory(TestPaths::private_data() / "arrietty_724.tiff");
	auto film = new_test_film("guess_crop_image_test1", content);

	BOOST_CHECK(guess_crop_by_brightness(film, content[0], 0.1, {}) == Crop(0, 0, 11, 11));
}


BOOST_AUTO_TEST_CASE (guess_crop_image_test2)
{
	auto content = content_factory(TestPaths::private_data() / "prophet_frame.tiff");
	auto film = new_test_film("guess_crop_image_test2", content);

	BOOST_CHECK(guess_crop_by_brightness(film, content[0], 0.1, {}) == Crop(0, 0, 22, 22));
}


BOOST_AUTO_TEST_CASE (guess_crop_image_test3)
{
	auto content = content_factory(TestPaths::private_data() / "pillarbox.png");
	auto film = new_test_film("guess_crop_image_test3", content);

	BOOST_CHECK(guess_crop_by_brightness(film, content[0], 0.1, {}) == Crop(113, 262, 0, 0));
}

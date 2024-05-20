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
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


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


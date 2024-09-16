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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/config.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(relative_paths_test)
{
	ConfigRestorer cr;
	Config::instance()->set_relative_paths(true);

	auto picture = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("relative_paths_test", { picture });
	film->write_metadata();

	auto film2 = std::make_shared<Film>(boost::filesystem::path("build/test/relative_paths_test"));
	film2->read_metadata();
	BOOST_REQUIRE_EQUAL(film2->content().size(), 1U);
	BOOST_REQUIRE(film2->content()[0]->paths_valid());
}


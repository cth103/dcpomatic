/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (pulldown_detect_test1)
{
	auto content = content_factory(TestPaths::private_data() / "greatbrain.mkv");
	auto film = new_test_film2("pulldown_detect_test1", content);

	BOOST_REQUIRE(static_cast<bool>(content[0]->video_frame_rate()));
	BOOST_CHECK_CLOSE(content[0]->video_frame_rate().get(), 23.976, 0.1);
}


/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(encode_xyz_from_prores_test)
{
	auto content = content_factory(TestPaths::private_data() / "dcp-o-matic_test_20250521_p3d65.mov")[0];
	auto film = new_test_film("encode_xyz_from_prores_test", { content });
	content->video->unset_colour_conversion();

	make_and_verify_dcp(film);

	check_one_frame(film->dir(film->dcp_name()), 0, TestPaths::private_data() / "dcp-o-matic_test_20250521_p3d65_frame0.j2c", 18);
}


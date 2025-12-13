/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file test/ffmpeg_dcp_test.cc
 *  @brief Test creation of a very simple DCP from some FFmpegContent (data/test.mp4).
 *  @ingroup feature
 *
 *  Also a quick test of Film::have_dcp ().
 */


#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(ffmpeg_dcp_test)
{
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	auto film = new_test_film("ffmpeg_dcp_test", { c });
	film->set_name("test_film2");
	make_and_verify_dcp(film);
}


/** Briefly test Film::cpls() */
BOOST_AUTO_TEST_CASE(ffmpeg_have_dcp_test, * boost::unit_test::depends_on("ffmpeg_dcp_test"))
{
	auto p = test_film_dir("ffmpeg_dcp_test");
	auto film = make_shared<Film>(p);
	film->read_metadata();
	BOOST_CHECK(!film->cpls().empty());

	p /= film->dcp_name();
	auto i = boost::filesystem::directory_iterator(p);
	while (i != boost::filesystem::directory_iterator() && !boost::algorithm::starts_with(i->path().filename().string(), "j2c")) {
		++i;
	}

	if (i != boost::filesystem::directory_iterator()) {
		boost::filesystem::remove(i->path());
	}

	BOOST_CHECK(film->cpls().empty());
}

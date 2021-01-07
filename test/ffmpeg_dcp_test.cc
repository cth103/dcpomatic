/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "test.h"

using std::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_dcp_test");
	film->set_name ("test_film2");
	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (c);

	BOOST_REQUIRE (!wait_for_jobs());

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs());
}

/** Briefly test Film::cpls().  Requires the output from ffmpeg_dcp_test above */
BOOST_AUTO_TEST_CASE (ffmpeg_have_dcp_test, * boost::unit_test::depends_on("ffmpeg_dcp_test"))
{
	boost::filesystem::path p = test_film_dir ("ffmpeg_dcp_test");
	shared_ptr<Film> film (new Film (p));
	film->read_metadata ();
	BOOST_CHECK (!film->cpls().empty());

	p /= film->dcp_name();
	boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (p);
	while (i != boost::filesystem::directory_iterator() && !boost::algorithm::starts_with (i->path().leaf().string(), "j2c")) {
		++i;
	}

	if (i != boost::filesystem::directory_iterator ()) {
		boost::filesystem::remove (i->path ());
	}

	BOOST_CHECK (film->cpls().empty());
}

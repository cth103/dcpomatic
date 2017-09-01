/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/subtitle_content.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Check for bug #1126 whereby making a new DCP using the same video asset as an old one
 *  corrupts the old one.
 */
BOOST_AUTO_TEST_CASE (remake_id_test)
{
	/* Make a DCP */
	shared_ptr<Film> film = new_test_film2 ("remake_id_test");
	shared_ptr<Content> content = content_factory(film, "test/data/flat_red.png").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Copy the video file */
	boost::filesystem::path first_video = video_file(film);
	boost::filesystem::copy_file (first_video, first_video.string() + ".copy");

	/* Make a new DCP with the same video file */
	film->set_name ("remake_id_test2");
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Check that the video in the first DCP hasn't changed */
	check_file (first_video, first_video.string() + ".copy");
}

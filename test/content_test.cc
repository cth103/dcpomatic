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

/** @file  test/content_test.cc
 *  @brief Tests which expose problems with certain pieces of content.
 *  @ingroup completedcp
 */

#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/content_factory.h"
#include "lib/content.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

/** There has been garbled audio with this piece of content */
BOOST_AUTO_TEST_CASE (content_test1)
{
	shared_ptr<Film> film = new_test_film ("content_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("content_test1");
	film->set_container (Ratio::from_id ("185"));

	shared_ptr<Content> content = content_factory(film, private_data / "demo_sound_bug.mkv").front ();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	boost::filesystem::path check;

	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator("build/test/content_test1/" + film->dcp_name());
		i != boost::filesystem::directory_iterator();
		++i) {

		if (i->path().leaf().string().substr(0, 4) == "pcm_") {
			check = *i;
		}
	}

	check_mxf_audio_file ("test/data/content_test1.mxf", check);
}

/** Taking some 23.976fps content and trimming 0.5s (in content time) from the start
 *  has failed in the past; ensure that this is fixed.
 */
BOOST_AUTO_TEST_CASE (content_test2)
{
	shared_ptr<Film> film = new_test_film2 ("content_test2");

	shared_ptr<Content> content = content_factory(film, "test/data/red_23976.mp4").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	content->set_trim_start(ContentTime::from_seconds(0.5));
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

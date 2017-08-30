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

BOOST_AUTO_TEST_CASE (remake_with_subtitle_test)
{
	shared_ptr<Film> film = new_test_film2 ("remake_with_subtitle_test");
	shared_ptr<FFmpegContent> content = dynamic_pointer_cast<FFmpegContent>(content_factory(film, private_data / "prophet_short_clip.mkv").front());
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	content->subtitle->set_burn (true);
	content->subtitle->set_use (true);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	boost::filesystem::remove_all (film->dir (film->dcp_name(), false));

	content->subtitle->set_use (false);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Nothing is being checked here so this test is not complete */
	DCPOMATIC_ASSERT (false);
}

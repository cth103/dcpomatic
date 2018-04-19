/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

/** Check for no crash when trimming DCP subtitles (#1275) */
BOOST_AUTO_TEST_CASE (subtitle_trim_test1)
{
	shared_ptr<Film> film = new_test_film2 ("subtitle_trim_test1");
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub5.xml"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	content->set_trim_end (ContentTime::from_seconds (2));
	film->write_metadata ();

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

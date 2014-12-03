/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  test/burnt_subtitle_test.cc
 *  @brief Test the burning of subtitles into the DCP.
 */

#include <boost/test/unit_test.hpp>
#include "lib/subrip_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "test.h"

using std::cout;
using boost::shared_ptr;

/** Build a small DCP with no picture and a single subtitle overlaid onto it from a SubRip file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_subrip)
{
	shared_ptr<Film> film = new_test_film ("burnt_subtitle_test_subrip");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_burn_subtitles (true);
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip2.srt"));
	content->set_use_subtitles (true);
	film->examine_and_add_content (content, true);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/burnt_subtitle_test_subrip", film->dir (film->dcp_name ()));
}

/** Build a small DCP with no picture and a single subtitle overlaid onto it from a DCP XML file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_dcp)
{
	shared_ptr<Film> film = new_test_film ("burnt_subtitle_test_dcp");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_burn_subtitles (true);
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub.xml"));
	content->set_use_subtitles (true);
	film->examine_and_add_content (content, true);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/burnt_subtitle_test_dcp", film->dir (film->dcp_name ()));
}

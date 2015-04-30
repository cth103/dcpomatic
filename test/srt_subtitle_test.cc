/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/subtitle_write_test.cc
 *  @brief Test writing DCPs with XML subtitles.
 */

#include "lib/film.h"
#include "lib/subrip_content.h"
#include "lib/dcp_content_type.h"
#include "lib/font.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

/** Make a very short DCP with a single subtitle from .srt with no specified fonts */
BOOST_AUTO_TEST_CASE (srt_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_burn_subtitles (false);
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->set_use_subtitles (true);
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/srt_subtitle_test", film->dir (film->dcp_name ()));
}

/** Same again but with a `font' specified */
BOOST_AUTO_TEST_CASE (srt_subtitle_test2)
{
	shared_ptr<Film> film = new_test_film ("srt_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_burn_subtitles (false);
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->set_use_subtitles (true);
	/* Use test/data/subrip2.srt as if it were a font file  */
	content->fonts().front()->file = "test/data/subrip2.srt";
	
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/srt_subtitle_test2", film->dir (film->dcp_name ()));
}


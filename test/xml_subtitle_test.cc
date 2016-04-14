/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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
 *  @brief Test creation of XML DCP subtitles.
 */

#include <boost/test/unit_test.hpp>
#include "lib/text_subtitle_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/subtitle_content.h"
#include "test.h"
#include <iostream>

using std::cout;
using boost::shared_ptr;

/** Build a small DCP with no picture and a single subtitle overlaid onto it */
BOOST_AUTO_TEST_CASE (xml_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("xml_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	/* Should be blank video with MXF subtitles */
	check_dcp ("test/data/xml_subtitle_test", film->dir (film->dcp_name ()));
}

/** Check the subtitle XML when there are two subtitle files in the project */
BOOST_AUTO_TEST_CASE (xml_subtitle_test2)
{
	shared_ptr<Film> film = new_test_film ("xml_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	film->set_sequence (false);
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->examine_and_add_content (content);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	content->set_position (DCPTime (0));
	film->make_dcp ();
	wait_for_jobs ();
	film->write_metadata ();

	check_dcp ("test/data/xml_subtitle_test2", film->dir (film->dcp_name ()));
}

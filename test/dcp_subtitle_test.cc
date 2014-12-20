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

/** @file  test/dcp_subtitle_test.cc
 *  @brief Test DCP subtitle content in various ways.
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "test.h"

using std::cout;
using boost::shared_ptr;

/** Test pass-through of a very simple DCP subtitle file */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_burn_subtitles (false);
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub.xml"));
	film->examine_and_add_content (content, true);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (2));

	content->set_use_subtitles (true);
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/dcp_subtitle_test", film->dir (film->dcp_name ()));
}

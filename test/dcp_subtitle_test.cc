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
#include "lib/dcp_content.h"
#include "lib/ratio.h"
#include "lib/dcp_decoder.h"
#include "lib/dcp_content_type.h"
#include "test.h"
#include <iostream>

using std::cout;
using std::list;
using boost::shared_ptr;

/** Test pass-through of a very simple DCP subtitle file */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub.xml"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds (2));

	content->set_use_subtitles (true);
	content->set_burn_subtitles (false);
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/dcp_subtitle_test", film->dir (film->dcp_name ()));
}

/** Test parsing of a subtitle within an existing DCP */
BOOST_AUTO_TEST_CASE (dcp_subtitle_within_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_within_dcp_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<DCPContent> content (new DCPContent (film, private_data / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	shared_ptr<DCPDecoder> decoder (new DCPDecoder (content, false));

	list<ContentTimePeriod> ctp = decoder->text_subtitles_during (
		ContentTimePeriod (
			ContentTime::from_seconds (25),
			ContentTime::from_seconds (26)
			),
		true
		);

	BOOST_REQUIRE_EQUAL (ctp.size(), 2);
	BOOST_CHECK_EQUAL (ctp.front().from, ContentTime::from_seconds (25 + 12 * 0.04));
	BOOST_CHECK_EQUAL (ctp.front().to, ContentTime::from_seconds (26 + 4 * 0.04));
	BOOST_CHECK_EQUAL (ctp.back().from, ContentTime::from_seconds (25 + 12 * 0.04));
	BOOST_CHECK_EQUAL (ctp.back().to, ContentTime::from_seconds (26 + 4 * 0.04));

	list<ContentTextSubtitle> subs = decoder->get_text_subtitles (
		ContentTimePeriod (
			ContentTime::from_seconds (25),
			ContentTime::from_seconds (26)
			),
		true
		);

	BOOST_REQUIRE_EQUAL (subs.size(), 1);
	BOOST_REQUIRE_EQUAL (subs.front().subs.size(), 2);
	BOOST_CHECK_EQUAL (subs.front().subs.front().text(), "Noch mal.");
	BOOST_CHECK_EQUAL (subs.front().subs.back().text(), "Encore une fois.");
}

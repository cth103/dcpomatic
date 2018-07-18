/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/render_text_test.cc
 *  @brief Check markup of subtitles for rendering.
 *  @ingroup specific
 */

#include "lib/render_text.h"
#include <dcp/subtitle_string.h>
#include <boost/test/unit_test.hpp>

static void
add (std::list<SubtitleString>& s, std::string text, bool italic, bool bold, bool underline)
{
	s.push_back (
		SubtitleString (
			dcp::SubtitleString (
				boost::optional<std::string> (),
				italic,
				bold,
				underline,
				dcp::Colour (255, 255, 255),
				42,
				1,
				dcp::Time (),
				dcp::Time (),
				1,
				dcp::HALIGN_LEFT,
				1,
				dcp::VALIGN_TOP,
				dcp::DIRECTION_LTR,
				text,
				dcp::NONE,
				dcp::Colour (0, 0, 0),
				dcp::Time (),
				dcp::Time ()
				)
			)
		);
}

/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test1)
{
	std::list<SubtitleString> s;
	add (s, "Hello", false, false, false);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}

/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test2)
{
	std::list<SubtitleString> s;
	add (s, "Hello", false, true, false);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span weight=\"bold\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}


/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test3)
{
	std::list<SubtitleString> s;
	add (s, "Hello", true, true, false);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span style=\"italic\" weight=\"bold\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}

/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test4)
{
	std::list<SubtitleString> s;
	add (s, "Hello", true, true, true);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span style=\"italic\" weight=\"bold\" underline=\"single\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}

/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test5)
{
	std::list<SubtitleString> s;
	add (s, "Hello", false, true, false);
	add (s, " world.", false, false, false);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span weight=\"bold\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span><span size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\"> world.</span>");
}

/** Test marked_up() in render_text.cc */
BOOST_AUTO_TEST_CASE (render_markup_test6)
{
	std::list<SubtitleString> s;
	add (s, "Hello", true, false, false);
	add (s, " world ", false, false, false);
	add (s, "we are bold.", false, true, false);
	BOOST_CHECK_EQUAL (marked_up (s, 1024, 1), "<span style=\"italic\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span><span size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\"> world </span><span weight=\"bold\" size=\"41472\" alpha=\"65535\" color=\"#FFFFFF\">we are bold.</span>");
}

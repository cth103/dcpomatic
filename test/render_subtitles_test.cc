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
 *  @ingroup feature
 */


#include "lib/image.h"
#include "lib/image_png.h"
#include "lib/render_text.h"
#include "lib/string_text.h"
#include <dcp/subtitle_string.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;


static void
add (std::list<StringText>& s, std::string text, bool italic, bool bold, bool underline)
{
	s.push_back (
		StringText (
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
				dcp::HAlign::LEFT,
				1,
				dcp::VAlign::TOP,
				0,
				dcp::Direction::LTR,
				text,
				dcp::Effect::NONE,
				dcp::Colour (0, 0, 0),
				dcp::Time (),
				dcp::Time (),
				0
				),
			2,
			std::shared_ptr<dcpomatic::Font>(),
			dcp::Standard::SMPTE
			)
		);
}


BOOST_AUTO_TEST_CASE (marked_up_test1)
{
	std::list<StringText> s;
	add (s, "Hello", false, false, false);
	BOOST_CHECK_EQUAL(marked_up(s, 1024, 1, ""), "<span size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}


BOOST_AUTO_TEST_CASE (marked_up_test2)
{
	std::list<StringText> s;
	add (s, "Hello", false, true, false);
	BOOST_CHECK_EQUAL(marked_up(s, 1024, 1, ""), "<span weight=\"bold\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}


BOOST_AUTO_TEST_CASE (marked_up_test3)
{
	std::list<StringText> s;
	add (s, "Hello", true, true, false);
	BOOST_CHECK_EQUAL(marked_up(s, 1024, 1, ""), "<span style=\"italic\" weight=\"bold\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}

BOOST_AUTO_TEST_CASE (marked_up_test4)
{
	std::list<StringText> s;
	add (s, "Hello", true, true, true);
	BOOST_CHECK_EQUAL(marked_up(s, 1024, 1, ""), "<span style=\"italic\" weight=\"bold\" underline=\"single\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span>");
}

BOOST_AUTO_TEST_CASE (marked_up_test5)
{
	std::list<StringText> s;
	add (s, "Hello", false, true, false);
	add (s, " world.", false, false, false);
	BOOST_CHECK_EQUAL (marked_up(s, 1024, 1, ""), "<span weight=\"bold\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span><span size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\"> world.</span>");
}

BOOST_AUTO_TEST_CASE (marked_up_test6)
{
	std::list<StringText> s;
	add (s, "Hello", true, false, false);
	add (s, " world ", false, false, false);
	add (s, "we are bold.", false, true, false);
	BOOST_CHECK_EQUAL (marked_up(s, 1024, 1, ""), "<span style=\"italic\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">Hello</span><span size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\"> world </span><span weight=\"bold\" size=\"41705\" alpha=\"65535\" color=\"#FFFFFF\">we are bold.</span>");
}


#if 0

BOOST_AUTO_TEST_CASE (render_text_test)
{
	auto dcp_string = dcp::SubtitleString(
		{}, false, false, false, dcp::Colour(255, 255, 255), 42, 1.0,
		dcp::Time(0, 0, 0, 0, 24), dcp::Time(0, 0, 1, 0, 24),
		0.5, dcp::HAlign::CENTER,
		0.5, dcp::VAlign::CENTER,
		dcp::Direction::LTR,
		"HÄllo jokers",
		dcp::Effect::NONE, dcp::Colour(0, 0, 0),
		{}, {},
		0
		);

	auto string_text = StringText(dcp_string, 0, shared_ptr<dcpomatic::Font>());

	auto images = render_text({ string_text }, dcp::Size(1998, 1080), {}, 24);

	BOOST_CHECK_EQUAL(images.size(), 1U);
	image_as_png(Image::ensure_alignment(images.front().image, Image::Alignment::PADDED)).write("build/test/render_text_test.png");
}

#endif

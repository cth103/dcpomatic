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

#include <boost/test/unit_test.hpp>
#include "lib/subrip.h"
#include "lib/subrip_content.h"

using std::list;
using std::string;
using boost::shared_ptr;

/** Test SubRip::convert_time */
BOOST_AUTO_TEST_CASE (subrip_time_test)
{
	BOOST_CHECK_EQUAL (SubRip::convert_time ("00:03:10,500"), rint (((3 * 60) + 10 + 0.5) * TIME_HZ));
	BOOST_CHECK_EQUAL (SubRip::convert_time ("04:19:51,782"), rint (((4 * 3600) + (19 * 60) + 51 + 0.782) * TIME_HZ));
}

/** Test SubRip::convert_coordinate */
BOOST_AUTO_TEST_CASE (subrip_coordinate_test)
{
	BOOST_CHECK_EQUAL (SubRip::convert_coordinate ("foo:42"), 42);
	BOOST_CHECK_EQUAL (SubRip::convert_coordinate ("X1:999"), 999);
}

/** Test SubRip::convert_content */
BOOST_AUTO_TEST_CASE (subrip_content_test)
{
	list<string> c;
	list<SubRipSubtitlePiece> p;
	
	c.push_back ("Hello world");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	c.clear ();

	c.push_back ("<b>Hello world</b>");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().bold, true);
	c.clear ();

	c.push_back ("<i>Hello world</i>");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().italic, true);
	c.clear ();

	c.push_back ("<u>Hello world</u>");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().underline, true);
	c.clear ();

	c.push_back ("{b}Hello world{/b}");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().bold, true);
	c.clear ();

	c.push_back ("{i}Hello world{/i}");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().italic, true);
	c.clear ();

	c.push_back ("{u}Hello world{/u}");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 1);
	BOOST_CHECK_EQUAL (p.front().text, "Hello world");
	BOOST_CHECK_EQUAL (p.front().underline, true);
	c.clear ();

	c.push_back ("<b>This is <i>nesting</i> of subtitles</b>");
	p = SubRip::convert_content (c);
	BOOST_CHECK_EQUAL (p.size(), 3);
	list<SubRipSubtitlePiece>::iterator i = p.begin ();	
	BOOST_CHECK_EQUAL (i->text, "This is ");
	BOOST_CHECK_EQUAL (i->bold, true);
	BOOST_CHECK_EQUAL (i->italic, false);
	++i;
	BOOST_CHECK_EQUAL (i->text, "nesting");
	BOOST_CHECK_EQUAL (i->bold, true);
	BOOST_CHECK_EQUAL (i->italic, true);
	++i;
	BOOST_CHECK_EQUAL (i->text, " of subtitles");
	BOOST_CHECK_EQUAL (i->bold, true);
	BOOST_CHECK_EQUAL (i->italic, false);
	++i;
	c.clear ();
}

/** Test parsing of full SubRip file content */
BOOST_AUTO_TEST_CASE (subrip_parse_test)
{
	SubRipContent content (shared_ptr<Film> (), "test/data/subrip.srt");
	content.examine (shared_ptr<Job> ());
}

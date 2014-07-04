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

/** @file  test/subrip_test.cc
 *  @brief Various tests of the subrip code.
 */

#include <boost/test/unit_test.hpp>
#include <dcp/subtitle_content.h>
#include "lib/subrip.h"
#include "lib/subrip_content.h"
#include "lib/subrip_decoder.h"
#include "lib/render_subtitles.h"
#include "test.h"

using std::list;
using std::vector;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Test SubRip::convert_time */
BOOST_AUTO_TEST_CASE (subrip_time_test)
{
	BOOST_CHECK_EQUAL (SubRip::convert_time ("00:03:10,500"), ContentTime::from_seconds ((3 * 60) + 10 + 0.5));
	BOOST_CHECK_EQUAL (SubRip::convert_time ("04:19:51,782"), ContentTime::from_seconds ((4 * 3600) + (19 * 60) + 51 + 0.782));
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
	shared_ptr<Film> film = new_test_film ("subrip_parse_test");
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip.srt"));
	content->examine (shared_ptr<Job> ());
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds ((3 * 60) + 56.471));

	SubRip s (content);

	vector<SubRipSubtitle>::const_iterator i = s._subtitles.begin();

	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((1 * 60) + 49.200));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((1 * 60) + 52.351));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "This is a subtitle, and it goes over two lines.");

	++i;
	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((1 * 60) + 52.440));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((1 * 60) + 54.351));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "We have emboldened this");
	BOOST_CHECK_EQUAL (i->pieces.front().bold, true);

	++i;
	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((1 * 60) + 54.440));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((1 * 60) + 56.590));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "And italicised this.");
	BOOST_CHECK_EQUAL (i->pieces.front().italic, true);

	++i;
	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((1 * 60) + 56.680));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((1 * 60) + 58.955));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "Shall I compare thee to a summers' day?");

	++i;
	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((2 * 60) + 0.840));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((2 * 60) + 3.400));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "Is this a dagger I see before me?");

	++i;
	BOOST_CHECK (i != s._subtitles.end ());
	BOOST_CHECK_EQUAL (i->period.from, ContentTime::from_seconds ((3 * 60) + 54.560));
	BOOST_CHECK_EQUAL (i->period.to, ContentTime::from_seconds ((3 * 60) + 56.471));
	BOOST_CHECK_EQUAL (i->pieces.size(), 1);
	BOOST_CHECK_EQUAL (i->pieces.front().text, "Hello world.");

	++i;
	BOOST_CHECK (i == s._subtitles.end ());
}

/** Test rendering of a SubRip subtitle */
BOOST_AUTO_TEST_CASE (subrip_render_test)
{
	shared_ptr<Film> film = new_test_film ("subrip_render_test");
	shared_ptr<SubRipContent> content (new SubRipContent (film, "test/data/subrip.srt"));
	content->examine (shared_ptr<Job> ());
	BOOST_CHECK_EQUAL (content->full_length(), DCPTime::from_seconds ((3 * 60) + 56.471));

	shared_ptr<SubRipDecoder> decoder (new SubRipDecoder (content));
	list<ContentTextSubtitle> cts = decoder->get_text_subtitles (
		ContentTimePeriod (
			ContentTime::from_seconds (109), ContentTime::from_seconds (110)
			), false
		);
	BOOST_CHECK_EQUAL (cts.size(), 1);

	PositionImage image = render_subtitles (cts.front().subs, dcp::Size (1998, 1080));
	write_image (image.image, "build/test/subrip_render_test.png");
	check_file ("build/test/subrip_render_test.png", "test/data/subrip_render_test.png");
}

/** Test of reading a typical .srt */
BOOST_AUTO_TEST_CASE (subrip_read_test)
{
	shared_ptr<Film> film = new_test_film ("subrip_read_test");
	boost::filesystem::path p = private_data / "sintel_en.srt";
	shared_ptr<SubRipContent> s (new SubRipContent (film, p));
	s->examine (shared_ptr<Job> ());
}

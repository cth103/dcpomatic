/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/text_content.h"
#include "lib/util.h"
#include "test.h"
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>


using std::string;
using std::vector;
using boost::shared_ptr;


vector<string> current_hints;


static
void
collect_hint (string hint)
{
	current_hints.push_back (hint);
}


static
vector<string>
get_hints (shared_ptr<Film> film)
{
	current_hints.clear ();
	Hints hints (film);
	hints.Hint.connect (collect_hint);
	hints.start ();
	hints.join ();
	while (signal_manager->ui_idle()) {}
	return current_hints;
}


static
void
check (TextType type, string name, string expected_hint)
{
	shared_ptr<Film> film = new_test_film2 (name);
	shared_ptr<Content> content = content_factory("test/data/" + name + ".srt").front();
	content->text.front()->set_type (type);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	vector<string> hints = get_hints (film);

	BOOST_REQUIRE_EQUAL (hints.size(), 1);
	BOOST_CHECK_EQUAL (hints[0], expected_hint);
}


BOOST_AUTO_TEST_CASE (hint_closed_caption_too_long)
{
	check (
		TEXT_CLOSED_CAPTION,
		"hint_closed_caption_too_long",
		String::compose("At least one of your closed caption lines has more than %1 characters.  It is advisable to make each line %1 characters at most in length.", MAX_CLOSED_CAPTION_LENGTH, MAX_CLOSED_CAPTION_LENGTH)
	      );
}


BOOST_AUTO_TEST_CASE (hint_many_closed_caption_lines)
{
	check (
		TEXT_CLOSED_CAPTION,
		"hint_many_closed_caption_lines",
		String::compose("Some of your closed captions span more than %1 lines, so they will be truncated.", MAX_CLOSED_CAPTION_LINES)
	      );
}


BOOST_AUTO_TEST_CASE (hint_subtitle_too_early)
{
	check (
		TEXT_OPEN_SUBTITLE,
		"hint_subtitle_too_early",
		"It is advisable to put your first subtitle at least 4 seconds after the start of the DCP to make sure it is seen."
		);
}


BOOST_AUTO_TEST_CASE (hint_short_subtitles)
{
	check (
		TEXT_OPEN_SUBTITLE,
		"hint_short_subtitles",
		"At least one of your subtitles lasts less than 15 frames.  It is advisable to make each subtitle at least 15 frames long."
		);
}


BOOST_AUTO_TEST_CASE (hint_subtitles_too_close)
{
	check (
		TEXT_OPEN_SUBTITLE,
		"hint_subtitles_too_close",
		"At least one of your subtitles starts less than 2 frames after the previous one.  It is advisable to make the gap between subtitles at least 2 frames."
	      );
}


BOOST_AUTO_TEST_CASE (hint_many_subtitle_lines)
{
	check (
		TEXT_OPEN_SUBTITLE,
		"hint_many_subtitle_lines",
		"At least one of your subtitles has more than 3 lines.  It is advisable to use no more than 3 lines."
	      );
}


BOOST_AUTO_TEST_CASE (hint_subtitle_too_long)
{
	check (
		TEXT_OPEN_SUBTITLE,
		"hint_subtitle_too_long",
		"At least one of your subtitle lines has more than 52 characters.  It is advisable to make each line 52 characters at most in length."
	      );
}


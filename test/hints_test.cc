/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/cross.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/hints.h"
#include "lib/text_content.h"
#include "lib/util.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


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
check (TextType type, string name, optional<string> expected_hint = optional<string>())
{
	auto film = new_test_film2 (name);
	auto content = content_factory("test/data/" + name + ".srt").front();
	content->text.front()->set_type (type);
	content->text.front()->set_language (dcp::LanguageTag("en-US"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	auto hints = get_hints (film);

	if (expected_hint) {
		BOOST_REQUIRE_EQUAL (hints.size(), 1U);
		BOOST_CHECK_EQUAL (hints[0], *expected_hint);
	} else {
		BOOST_CHECK (hints.empty());
	}
}


BOOST_AUTO_TEST_CASE (hint_closed_caption_too_long)
{
	check (
		TextType::CLOSED_CAPTION,
		"hint_closed_caption_too_long",
		String::compose("At least one of your closed caption lines has more than %1 characters.  It is advisable to make each line %1 characters at most in length.", MAX_CLOSED_CAPTION_LENGTH, MAX_CLOSED_CAPTION_LENGTH)
	      );
}


BOOST_AUTO_TEST_CASE (hint_many_closed_caption_lines)
{
	check (
		TextType::CLOSED_CAPTION,
		"hint_many_closed_caption_lines",
		String::compose("Some of your closed captions span more than %1 lines, so they will be truncated.", MAX_CLOSED_CAPTION_LINES)
	      );
}


BOOST_AUTO_TEST_CASE (hint_subtitle_too_early)
{
	check (
		TextType::OPEN_SUBTITLE,
		"hint_subtitle_too_early",
		string("It is advisable to put your first subtitle at least 4 seconds after the start of the DCP to make sure it is seen.")
		);
}


BOOST_AUTO_TEST_CASE (hint_short_subtitles)
{
	check (
		TextType::OPEN_SUBTITLE,
		"hint_short_subtitles",
		string("At least one of your subtitles lasts less than 15 frames.  It is advisable to make each subtitle at least 15 frames long.")
		);
}


BOOST_AUTO_TEST_CASE (hint_subtitles_too_close)
{
	check (
		TextType::OPEN_SUBTITLE,
		"hint_subtitles_too_close",
		string("At least one of your subtitles starts less than 2 frames after the previous one.  It is advisable to make the gap between subtitles at least 2 frames.")
	      );
}


BOOST_AUTO_TEST_CASE (hint_many_subtitle_lines)
{
	check (
		TextType::OPEN_SUBTITLE,
		"hint_many_subtitle_lines",
		string("At least one of your subtitles has more than 3 lines.  It is advisable to use no more than 3 lines.")
	      );
}


BOOST_AUTO_TEST_CASE (hint_subtitle_too_long)
{
	check (
		TextType::OPEN_SUBTITLE,
		"hint_subtitle_too_long",
		string("At least one of your subtitle lines has more than 52 characters.  It is advisable to make each line 52 characters at most in length.")
	      );
}


BOOST_AUTO_TEST_CASE (hint_subtitle_mxf_too_big)
{
	string const name = "hint_subtitle_mxf_too_big";

	auto film = new_test_film2 (name);
	auto content = content_factory("test/data/" + name + ".srt").front();
	content->text.front()->set_type (TextType::OPEN_SUBTITLE);
	content->text.front()->set_language (dcp::LanguageTag("en-US"));
	for (int i = 1; i < 512; ++i) {
		auto font = make_shared<dcpomatic::Font>(String::compose("font_%1", i));
		font->set_file ("test/data/LiberationSans-Regular.ttf");
		content->text.front()->add_font(font);
	}
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	auto hints = get_hints (film);

	BOOST_REQUIRE_EQUAL (hints.size(), 1U);
	BOOST_CHECK_EQUAL (
		hints[0],
		"At least one of your subtitle files is larger than " MAX_TEXT_MXF_SIZE_TEXT " in total.  "
		"You should divide the DCP into shorter reels."
		);
}


BOOST_AUTO_TEST_CASE (hint_closed_caption_xml_too_big)
{
	string const name = "hint_closed_caption_xml_too_big";

	auto film = new_test_film2 (name);

	auto ccap = fopen_boost (String::compose("build/test/%1.srt", name), "w");
	BOOST_REQUIRE (ccap);
	for (int i = 0; i < 2048; ++i) {
		fprintf(ccap, "%d\n", i + 1);
		int second = i * 2;
		int minute = second % 60;
		fprintf(ccap, "00:%02d:%02d,000 --> 00:%02d:%02d,000\n", minute, second, minute, second + 1);
		fprintf(ccap, "Here are some closed captions.\n\n");
	}
	fclose (ccap);

	auto content = content_factory("build/test/" + name + ".srt").front();
	content->text.front()->set_type (TextType::CLOSED_CAPTION);
	content->text.front()->set_language (dcp::LanguageTag("en-US"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	auto hints = get_hints (film);

	BOOST_REQUIRE_EQUAL (hints.size(), 1U);
	BOOST_CHECK_EQUAL (
		hints[0],
		"At least one of your closed caption files' XML part is larger than " MAX_CLOSED_CAPTION_XML_SIZE_TEXT ".  "
		"You should divide the DCP into shorter reels."
		);
}


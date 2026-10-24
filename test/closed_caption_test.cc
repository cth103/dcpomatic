/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/layout_closed_captions.h"
#include "lib/string_text.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_text_asset.h>
#include <dcp/text_asset.h>
#include <dcp/text_string.h>
#include <boost/test/unit_test.hpp>


using std::list;
using std::make_shared;
using std::vector;


/** Basic test that Interop closed captions are written */
BOOST_AUTO_TEST_CASE (closed_caption_test1)
{
	Cleanup cl;

	auto content = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("closed_caption_test1", { content }, &cl);

	content->only_text()->set_type (TextType::CLOSED_CAPTION);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* Just check to see that there's a CCAP in the CPL: this
	   check could be better!
	*/

	dcp::DCP check (film->dir(film->dcp_name()));
	check.read ();

	BOOST_REQUIRE_EQUAL (check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (check.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (!check.cpls().front()->reels().front()->closed_captions().empty());

	cl.run ();
}

/** Test multiple closed captions */
BOOST_AUTO_TEST_CASE (closed_caption_test2)
{
	Cleanup cl;
	auto content1 = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto content2 = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	auto content3 = make_shared<StringTextFileContent>("test/data/subrip3.srt");
	auto film = new_test_film("closed_caption_test2", { content1, content2, content3 }, &cl);

	content1->only_text()->set_type (TextType::CLOSED_CAPTION);
	content1->only_text()->set_dcp_track (DCPTextTrack("First track", dcp::LanguageTag("fr-FR")));
	content2->only_text()->set_type (TextType::CLOSED_CAPTION);
	content2->only_text()->set_dcp_track (DCPTextTrack("Second track", dcp::LanguageTag("de-DE")));
	content3->only_text()->set_type (TextType::CLOSED_CAPTION);
	content3->only_text()->set_dcp_track (DCPTextTrack("Third track", dcp::LanguageTag("it-IT")));

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION_BV21,
			dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		},
		true,
		/* Clairmeta gives an error about having duplicate ClosedCaption entries,
		 * which seems wrong.
		 */
		false
		);

	dcp::DCP check (film->dir(film->dcp_name()));
	check.read ();

	BOOST_REQUIRE_EQUAL (check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (check.cpls().front()->reels().size(), 1U);
	auto ccaps = check.cpls().front()->reels().front()->closed_captions();
	BOOST_REQUIRE_EQUAL (ccaps.size(), 3U);

	auto i = ccaps.begin ();
	BOOST_CHECK_EQUAL ((*i)->annotation_text().get_value_or(""), "First track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "fr-FR");
	++i;
	BOOST_CHECK_EQUAL ((*i)->annotation_text().get_value_or(""), "Second track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "de-DE");
	++i;
	BOOST_CHECK_EQUAL ((*i)->annotation_text().get_value_or(""), "Third track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "it-IT");

	cl.run ();
}


BOOST_AUTO_TEST_CASE(closed_captions_with_italics_test)
{
	vector<StringText> text = {
		{
			{
				{}, false, true, true, {}, 42, 1, dcp::Time(0, 0, 0, 0, 24), dcp::Time(0, 0, 1, 0, 24),
				0, dcp::HAlign::CENTER,
				0, dcp::VAlign::CENTER,
				0,
				{}, dcp::Direction::LTR, "Hello ", dcp::Effect::NONE, {}, {}, {}, 0, {}
			}, 1, {}, dcp::SubtitleStandard::SMPTE_2014
		},
		{
			{
				{}, true, true, true, {}, 42, 1, dcp::Time(0, 0, 0, 0, 24), dcp::Time(0, 0, 1, 0, 24),
				0, dcp::HAlign::CENTER,
				0, dcp::VAlign::CENTER,
				0,
				{}, dcp::Direction::LTR, "world", dcp::Effect::NONE, {}, {}, {}, 0, {}
			}, 1, {}, dcp::SubtitleStandard::SMPTE_2014
		},
		{
			{
				{}, false, true, true, {}, 42, 1, dcp::Time(0, 0, 0, 0, 24), dcp::Time(0, 0, 1, 0, 24),
				0, dcp::HAlign::CENTER,
				4, dcp::VAlign::CENTER,
				0,
				{}, dcp::Direction::LTR, "It's a new line", dcp::Effect::NONE, {}, {}, {}, 0, {}
			}, 1, {}, dcp::SubtitleStandard::SMPTE_2014
		}
	};

	auto const strings = layout_closed_captions(text);
	BOOST_REQUIRE_EQUAL(strings.size(), 2U);
	BOOST_CHECK_EQUAL(strings[0], "Hello world");
	BOOST_CHECK_EQUAL(strings[1], "It's a new line");
}


/** Some servers show closed captions on screen if there is no open subtitle track, so a DCP with
 *  closed captions and no open subtitles gets an empty open subtitle track.
 */
BOOST_AUTO_TEST_CASE(closed_captions_get_empty_open_subtitles)
{
	auto content = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("closed_captions_get_empty_open_subtitles", { content });
	film->set_name("Captions");
	content->only_text()->set_type(TextType::CLOSED_CAPTION);
	content->only_text()->set_dcp_track(DCPTextTrack("Captions", dcp::LanguageTag("en-US")));
	auto const name = film->isdcf_name(false, false);

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* The ISDCF name does not change: it still says there are no subtitles */
	BOOST_CHECK_EQUAL(film->isdcf_name(false, false), name);

	dcp::DCP check(film->dir(film->dcp_name()));
	check.read();
	BOOST_REQUIRE_EQUAL(check.cpls().size(), 1U);
	auto reels = check.cpls().front()->reels();
	BOOST_REQUIRE(!reels.empty());
	for (auto reel: reels) {
		BOOST_REQUIRE(!reel->closed_captions().empty());
		auto subtitle = reel->main_subtitle();
		BOOST_REQUIRE(subtitle);
		BOOST_REQUIRE(subtitle->asset());
		BOOST_CHECK_EQUAL(subtitle->language().get_value_or(""), "zxx");
		auto texts = subtitle->asset()->texts();
		BOOST_REQUIRE_EQUAL(texts.size(), 1U);
		auto string = std::dynamic_pointer_cast<const dcp::TextString>(texts.front());
		BOOST_REQUIRE(string);
		BOOST_CHECK_EQUAL(string->text(), " ");
	}
}


/** The dummy text of the empty open subtitle track of a first reel that is at least 5 s long is 4 s in, as Bv2.1
 *  7.2.4 asks, but that of the next reels is at the start
 */
BOOST_AUTO_TEST_CASE(closed_captions_empty_open_subtitles_start_after_4s)
{
	auto red = content_factory("test/data/flat_red.png")[0];
	auto green = content_factory("test/data/flat_green.png")[0];
	auto content = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("closed_captions_empty_open_subtitles_start_after_4s", { red, green, content });
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	content->set_position(film, dcpomatic::DCPTime());
	content->only_text()->set_type(TextType::CLOSED_CAPTION);
	content->only_text()->set_dcp_track(DCPTextTrack("Captions", dcp::LanguageTag("en-US")));

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	dcp::DCP check(film->dir(film->dcp_name()));
	check.read();
	BOOST_REQUIRE_EQUAL(check.cpls().size(), 1U);
	auto reels = check.cpls().front()->reels();
	BOOST_REQUIRE(reels.size() >= 2U);
	for (size_t i = 0; i < reels.size(); ++i) {
		auto subtitle = reels[i]->main_subtitle();
		BOOST_REQUIRE(subtitle);
		auto texts = subtitle->asset()->texts();
		BOOST_REQUIRE_EQUAL(texts.size(), 1U);
		BOOST_CHECK(texts.front()->in() == dcp::Time(0, 0, i == 0 ? 4 : 0, 0, 24));
	}
}


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


#include "lib/film.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_closed_caption_asset.h>
#include <boost/test/unit_test.hpp>


using std::list;
using std::make_shared;


/** Basic test that Interop closed captions are written */
BOOST_AUTO_TEST_CASE (closed_caption_test1)
{
	Cleanup cl;

	auto content = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film2 ("closed_caption_test1", { content }, &cl);

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
	auto film = new_test_film2 ("closed_caption_test2", { content1, content2, content3 }, &cl);

	content1->only_text()->set_type (TextType::CLOSED_CAPTION);
	content1->only_text()->set_dcp_track (DCPTextTrack("First track", dcp::LanguageTag("fr-FR")));
	content2->only_text()->set_type (TextType::CLOSED_CAPTION);
	content2->only_text()->set_dcp_track (DCPTextTrack("Second track", dcp::LanguageTag("de-DE")));
	content3->only_text()->set_type (TextType::CLOSED_CAPTION);
	content3->only_text()->set_dcp_track (DCPTextTrack("Third track", dcp::LanguageTag("it-IT")));

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
		}
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

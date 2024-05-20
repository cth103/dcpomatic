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


/** @file  test/subtitle_language_test.cc
 *  @brief Test that subtitle language information is correctly written to DCPs.
 */


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/language_tag.h>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE (subtitle_language_interop_test)
{
	string const name = "subtitle_language_interop_test";
	auto fr = content_factory("test/data/frames.srt");
	auto film = new_test_film(name, fr);

	fr[0]->only_text()->set_language(dcp::LanguageTag("fr"));
	film->set_interop (true);
	film->set_audio_channels(6);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION
		},
		false,
		/* clairmeta raises errors about subtitle spacing/duration */
		false
		);

	check_dcp(String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()));
}


BOOST_AUTO_TEST_CASE (subtitle_language_smpte_test)
{
	string const name = "subtitle_language_smpte_test";
	auto fr = content_factory("test/data/frames.srt");
	auto film = new_test_film(name, fr);

	fr[0]->only_text()->set_language(dcp::LanguageTag("fr"));
	film->set_interop (false);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* This test is concerned with the subtitles, so we'll ignore any
	 * differences in sound between the DCP and the reference to avoid test
	 * failures for unrelated reasons.
	 */
	check_dcp(String::compose("test/data/%1", name), String::compose("build/test/%1/%2", name, film->dcp_name()), true);
}


BOOST_AUTO_TEST_CASE(subtitle_language_in_cpl_test)
{
	auto subs = content_factory("test/data/frames.srt")[0];
	auto video1 = content_factory("test/data/flat_red.png")[0];
	auto video2 = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film(boost::unit_test::framework::current_test_unit().full_name(), { subs, video1, video2 });
	video2->set_position(film, dcpomatic::DCPTime::from_seconds(5));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	subs->only_text()->set_language(dcp::LanguageTag("fr"));

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING
		});

	cxml::Document cpl("CompositionPlaylist");
	cpl.read_file(find_file(film->dir(film->dcp_name()), "cpl_"));

	for (auto reel: cpl.node_child("ReelList")->node_children("Reel")) {
		auto subtitle = reel->node_child("AssetList")->node_child("MainSubtitle");
		BOOST_REQUIRE(subtitle);
		BOOST_CHECK(subtitle->optional_node_child("Language"));
	}
}


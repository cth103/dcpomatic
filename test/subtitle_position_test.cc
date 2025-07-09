/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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
#include "lib/make_dcp.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/interop_text_asset.h>
#include <dcp/language_tag.h>
#include <dcp/smpte_text_asset.h>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::shared_ptr;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE(srt_correctly_placed_in_interop)
{
	string const name = "srt_in_interop_position_test";
	auto fr = content_factory("test/data/short.srt");
	auto film = new_test_film(name, fr);
	fr[0]->only_text()->set_language(dcp::LanguageTag("de"));

	film->set_interop(true);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	auto output = subtitle_file(film);

	dcp::InteropTextAsset asset(output);
	auto output_subs = asset.texts();
	BOOST_REQUIRE_EQUAL(output_subs.size(), 1U);

	BOOST_CHECK(output_subs[0]->v_align() == dcp::VAlign::BOTTOM);
	BOOST_CHECK_CLOSE(output_subs[0]->v_position(), 0.172726989, 1e-3);
}


BOOST_AUTO_TEST_CASE(srt_correctly_placed_in_smpte)
{
	string const name = "srt_in_smpte_position_test";
	auto fr = content_factory("test/data/short.srt");
	auto film = new_test_film(name, fr);

	fr[0]->text[0]->set_language(dcp::LanguageTag("en"));
	film->set_interop(false);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	auto output = subtitle_file(film);

	dcp::SMPTETextAsset asset(output);
	auto output_subs = asset.texts();
	BOOST_REQUIRE_EQUAL(output_subs.size(), 1U);

	BOOST_CHECK(output_subs[0]->v_align() == dcp::VAlign::BOTTOM);
	BOOST_CHECK_CLOSE(output_subs[0]->v_position(), 0.172726989, 1e-3);
}


/** Make a DCP from some DCP subtitles and check the vertical alignment */
static
void
vpos_test(dcp::VAlign reference, float position, dcp::SubtitleStandard from, dcp::Standard to)
{
	string standard;
	switch (from) {
		case dcp::SubtitleStandard::SMPTE_2007:
		case dcp::SubtitleStandard::SMPTE_2010:
			standard = "smpte_2010";
			break;
		case dcp::SubtitleStandard::INTEROP:
			standard = "interop";
			break;
		case dcp::SubtitleStandard::SMPTE_2014:
			standard = "smpte_2014";
			break;
	}

	auto name = fmt::format("vpos_test_{}_{}", standard, valign_to_string(reference));
	auto in = content_factory(fmt::format("test/data/{}.xml", name));
	auto film = new_test_film(name, in);

	film->set_interop(to == dcp::Standard::INTEROP);

	film->write_metadata();
	make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_REQUIRE(!wait_for_jobs());

	auto out = subtitle_file(film);
	vector<shared_ptr<const dcp::Text>> subtitles;
	if (to == dcp::Standard::INTEROP) {
		dcp::InteropTextAsset asset(out);
		subtitles = asset.texts();
	} else {
		dcp::SMPTETextAsset asset(out);
		subtitles = asset.texts();
	}

	BOOST_REQUIRE_EQUAL(subtitles.size(), 1U);

	BOOST_CHECK(subtitles[0]->v_align() == reference);
	BOOST_CHECK_CLOSE(subtitles[0]->v_position(), position, 2);
}


BOOST_AUTO_TEST_CASE(subtitles_correctly_placed_with_all_references)
{
	constexpr auto baseline_to_bottom = 0.00925926;
	constexpr auto height = 0.0462963;

	/* Interop source */
	auto from = dcp::SubtitleStandard::INTEROP;

	// -> Interop
	vpos_test(dcp::VAlign::TOP, 0.2, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::CENTER, 0.11, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::BOTTOM, 0.08, from, dcp::Standard::INTEROP);

	// -> SMPTE (2014)
	vpos_test(dcp::VAlign::TOP, 0.2, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::CENTER, 0.11, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::BOTTOM, 0.08, from, dcp::Standard::SMPTE);

	/* SMPTE 2010 source */
	from = dcp::SubtitleStandard::SMPTE_2010;

	// -> Interop
	vpos_test(dcp::VAlign::TOP, 0.1 + height - baseline_to_bottom, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::CENTER, 0.15 + (height / 2) - baseline_to_bottom, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::BOTTOM, 0.10 + baseline_to_bottom, from, dcp::Standard::INTEROP);

	// -> SMPTE (2014)
	vpos_test(dcp::VAlign::TOP, 0.1 + height - baseline_to_bottom, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::CENTER, 0.15 + (height / 2) - baseline_to_bottom, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::BOTTOM, 0.10 + baseline_to_bottom, from, dcp::Standard::SMPTE);

	/* SMPTE 2014 source */
	from = dcp::SubtitleStandard::SMPTE_2014;

	// -> Interop
	vpos_test(dcp::VAlign::TOP, 0.2, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::CENTER, 0.11, from, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::BOTTOM, 0.08, from, dcp::Standard::INTEROP);

	// -> SMPTE (2014)
	vpos_test(dcp::VAlign::TOP, 0.2, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::CENTER, 0.11, from, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::BOTTOM, 0.08, from, dcp::Standard::SMPTE);
}


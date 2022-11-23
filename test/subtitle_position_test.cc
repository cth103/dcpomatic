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
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/language_tag.h>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::string;
using std::vector;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE(interop_correctly_placed_in_interop)
{
	string const name = "interop_in_interop_position_test";
	auto fr = content_factory("test/data/dcp_sub.xml");
	auto film = new_test_film2(name, fr);

	film->set_interop(true);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION
		});

	auto output = subtitle_file(film);
	dcp::InteropSubtitleAsset asset(output);
	auto output_subs = asset.subtitles();
	BOOST_REQUIRE_EQUAL(output_subs.size(), 1U);

	/* The input subtitle should have been left alone */
	BOOST_CHECK(output_subs[0]->v_align() == dcp::VAlign::BOTTOM);
	BOOST_CHECK_CLOSE(output_subs[0]->v_position(), 0.08, 1e-3);
}


BOOST_AUTO_TEST_CASE(interop_correctly_placed_in_smpte)
{
	string const name = "interop_in_smpte_position_test";
	auto fr = content_factory("test/data/dcp_sub.xml");
	auto film = new_test_film2(name, fr);

	film->set_interop(false);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	auto output = subtitle_file(film);
	dcp::SMPTESubtitleAsset asset(output);
	auto output_subs = asset.subtitles();
	BOOST_REQUIRE_EQUAL(output_subs.size(), 1U);

	BOOST_CHECK(output_subs[0]->v_align() == dcp::VAlign::BOTTOM);
	BOOST_CHECK_CLOSE(output_subs[0]->v_position(), 0.07074, 2);
}


BOOST_AUTO_TEST_CASE(smpte_correctly_placed_in_interop)
{
	string const name = "smpte_in_interop_position_test";
	auto fr = content_factory("test/data/short.srt");
	auto film = new_test_film2(name, fr);

	film->set_interop(true);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	auto output = subtitle_file(film);
	dcp::InteropSubtitleAsset asset(output);
	auto output_subs = asset.subtitles();
	BOOST_REQUIRE_EQUAL(output_subs.size(), 1U);

	BOOST_CHECK(output_subs[0]->v_align() == dcp::VAlign::TOP);
	BOOST_CHECK_CLOSE(output_subs[0]->v_position(), 0.87079, 1e-3);
}


static
void
vpos_test(dcp::VAlign reference, float position, dcp::Standard from, dcp::Standard to)
{
	string standard = from == dcp::Standard::INTEROP ? "interop" : "smpte";
	auto name = String::compose("vpos_test_%1_%2", standard, valign_to_string(reference));
	auto in = content_factory(String::compose("test/data/%1.xml", name));
	auto film = new_test_film2(name, in);

	film->set_interop(to == dcp::Standard::INTEROP);

	film->write_metadata();
	make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_REQUIRE(!wait_for_jobs());

	auto out = subtitle_file(film);
	vector<shared_ptr<const dcp::Subtitle>> subtitles;
	if (to == dcp::Standard::INTEROP) {
		dcp::InteropSubtitleAsset asset(out);
		subtitles = asset.subtitles();
	} else {
		dcp::SMPTESubtitleAsset asset(out);
		subtitles = asset.subtitles();
	}

	BOOST_REQUIRE_EQUAL(subtitles.size(), 1U);

	BOOST_CHECK(subtitles[0]->v_align() == reference);
	BOOST_CHECK_CLOSE(subtitles[0]->v_position(), position, 2);
}


BOOST_AUTO_TEST_CASE(subtitles_correctly_placed_with_all_references)
{
	constexpr auto baseline_to_bottom = 0.00925926;
	constexpr auto height = 0.0462963;

	vpos_test(dcp::VAlign::TOP, 0.2 - height + baseline_to_bottom, dcp::Standard::INTEROP, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::CENTER, 0.11 - (height / 2) + baseline_to_bottom, dcp::Standard::INTEROP, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::BOTTOM, 0.08 - baseline_to_bottom, dcp::Standard::INTEROP, dcp::Standard::SMPTE);
	vpos_test(dcp::VAlign::TOP, 0.1 + height - baseline_to_bottom, dcp::Standard::SMPTE, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::CENTER, 0.15 + (height / 2) - baseline_to_bottom, dcp::Standard::SMPTE, dcp::Standard::INTEROP);
	vpos_test(dcp::VAlign::BOTTOM, 0.10 + baseline_to_bottom, dcp::Standard::SMPTE, dcp::Standard::INTEROP);
}


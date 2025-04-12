/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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
#include "lib/dcp_subtitle_content.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/interop_text_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_text_asset.h>
#include <boost/test/unit_test.hpp>


using std::list;
using std::make_shared;
using std::string;
using boost::optional;


/* Check that timings are done correctly for multi-reel DCPs with PNG subs */
BOOST_AUTO_TEST_CASE (subtitle_reel_test)
{
	auto film = new_test_film("subtitle_reel_test");
	film->set_interop (true);
	auto red_a = make_shared<ImageContent>("test/data/flat_red.png");
	auto red_b = make_shared<ImageContent>("test/data/flat_red.png");
	auto sub_a = make_shared<DCPSubtitleContent>("test/data/png_subs/subs.xml");
	auto sub_b = make_shared<DCPSubtitleContent>("test/data/png_subs/subs.xml");

	film->examine_and_add_content (red_a);
	film->examine_and_add_content (red_b);
	film->examine_and_add_content (sub_a);
	film->examine_and_add_content (sub_b);

	BOOST_REQUIRE (!wait_for_jobs());

	red_a->set_position (film, dcpomatic::DCPTime());
	red_a->video->set_length (240);
	sub_a->set_position (film, dcpomatic::DCPTime());
	sub_a->only_text()->set_language(dcp::LanguageTag("de"));
	red_b->set_position (film, dcpomatic::DCPTime::from_seconds(10));
	red_b->video->set_length (240);
	sub_b->set_position (film, dcpomatic::DCPTime::from_seconds(10));
	sub_b->only_text()->set_language(dcp::LanguageTag("de"));

	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_STANDARD});

	dcp::DCP dcp ("build/test/subtitle_reel_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls().front();

	auto reels = cpl->reels ();
	BOOST_REQUIRE_EQUAL (reels.size(), 2U);
	auto i = reels.begin ();
	BOOST_REQUIRE ((*i)->main_subtitle());
	BOOST_REQUIRE ((*i)->main_subtitle()->asset());
	auto A = std::dynamic_pointer_cast<dcp::InteropTextAsset>((*i)->main_subtitle()->asset());
	BOOST_REQUIRE (A);
	++i;
	BOOST_REQUIRE ((*i)->main_subtitle());
	BOOST_REQUIRE ((*i)->main_subtitle()->asset());
	auto B = std::dynamic_pointer_cast<dcp::InteropTextAsset>((*i)->main_subtitle()->asset());
	BOOST_REQUIRE (B);

	BOOST_REQUIRE_EQUAL(A->texts().size(), 1U);
	BOOST_REQUIRE_EQUAL(B->texts().size(), 1U);

	/* These times should be the same as they are should be offset from the start of the reel */
	BOOST_CHECK(A->texts().front()->in() == B->texts().front()->in());
}



/** Check that with a SMPTE DCP if we have subtitles in one reel, all reels have a
 *  SubtitleAsset (even if it's empty); SMPTE Bv2.1 section 8.3.1.
 */
BOOST_AUTO_TEST_CASE (subtitle_in_all_reels_test)
{
	auto film = new_test_film("subtitle_in_all_reels_test");
	film->set_interop (false);
	film->set_sequence (false);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	for (int i = 0; i < 3; ++i) {
		auto video = content_factory("test/data/flat_red.png")[0];
		film->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (15 * 24);
		video->set_position (film, dcpomatic::DCPTime::from_seconds(15 * i));
	}
	auto subs = content_factory("test/data/15s.srt")[0];
	film->examine_and_add_content (subs);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING
		});

	dcp::DCP dcp ("build/test/subtitle_in_all_reels_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 3U);

	for (auto i: cpl->reels()) {
		BOOST_CHECK (i->main_subtitle());
	}
}


/** Check that with a SMPTE DCP if we have closed captions in one reel, all reels have a
 *  ClosedCaptionAssets for the same set of tracks (even if they are empty); SMPTE Bv2.1 section 8.3.1.
 */
BOOST_AUTO_TEST_CASE (closed_captions_in_all_reels_test)
{
	auto film = new_test_film("closed_captions_in_all_reels_test");
	film->set_interop (false);
	film->set_sequence (false);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	for (int i = 0; i < 3; ++i) {
		auto video = content_factory("test/data/flat_red.png")[0];
		film->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (15 * 24);
		video->set_position (film, dcpomatic::DCPTime::from_seconds(15 * i));
	}

	auto ccap1 = content_factory("test/data/15s.srt")[0];
	film->examine_and_add_content (ccap1);
	BOOST_REQUIRE (!wait_for_jobs());
	ccap1->text.front()->set_type (TextType::CLOSED_CAPTION);
	ccap1->text.front()->set_dcp_track (DCPTextTrack("Test", dcp::LanguageTag("de-DE")));

	auto ccap2 = content_factory("test/data/15s.srt")[0];
	film->examine_and_add_content (ccap2);
	BOOST_REQUIRE (!wait_for_jobs());
	ccap2->text.front()->set_type (TextType::CLOSED_CAPTION);
	ccap2->text.front()->set_dcp_track (DCPTextTrack("Other", dcp::LanguageTag("en-GB")));

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING
		},
		true,
		/* ClairMeta gives an error with multiple ClosedCaption assets */
		false
		);

	dcp::DCP dcp ("build/test/closed_captions_in_all_reels_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 3U);

	for (auto i: cpl->reels()) {
		BOOST_REQUIRE_EQUAL (i->closed_captions().size(), 2U);
		auto first = i->closed_captions().front()->language();
		auto second = i->closed_captions().back()->language();
		BOOST_REQUIRE (first);
		BOOST_REQUIRE (second);
		BOOST_CHECK (
			(*first == "en-GB" && *second == "de-DE") ||
			(*first == "de-DE" && *second == "en-GB")
			);
	}
}


BOOST_AUTO_TEST_CASE (subtitles_split_at_reel_boundaries)
{
	auto film = new_test_film("subtitles_split_at_reel_boundaries");
	film->set_interop (true);

	film->set_sequence (false);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);

	for (int i = 0; i < 3; ++i) {
		auto video = content_factory("test/data/flat_red.png")[0];
		film->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (15 * 24);
		video->set_position (film, dcpomatic::DCPTime::from_seconds(15 * i));
	}

	auto subtitle = content_factory("test/data/45s.srt")[0];
	film->examine_and_add_content (subtitle);
	BOOST_REQUIRE (!wait_for_jobs());
	subtitle->only_text()->set_language(dcp::LanguageTag("de"));

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	dcp::DCP dcp (film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 3U);

	for (auto i: cpl->reels()) {
		auto reel_sub = i->main_subtitle();
		BOOST_REQUIRE (reel_sub);
		auto sub = reel_sub->asset();
		BOOST_REQUIRE (sub);
		BOOST_CHECK_EQUAL(sub->texts().size(), 1U);
	}
}


BOOST_AUTO_TEST_CASE(bad_subtitle_not_created_at_reel_boundaries)
{
	boost::filesystem::path const srt = "build/test/bad_subtitle_not_created_at_reel_boundaries.srt";
	dcp::write_string_to_file("1\n00:00:10,000 -> 00:00:20,000\nHello world", srt);
	auto content = content_factory(srt)[0];

	auto film = new_test_film("bad_subtitle_not_created_at_reel_boundaries", { content });
	film->set_reel_type(ReelType::CUSTOM);
	content->text[0]->set_language(dcp::LanguageTag("de-DE"));
	/* This is 1 frame after the start of the subtitle */
	film->set_custom_reel_boundaries({dcpomatic::DCPTime::from_frames(241, 24)});

	/* This is a tricky situation and the way DoM deals with it gives two Bv2.1
	 * warnings, but these are "should" not "shall" so I think it's OK.
	 */
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
		});
}


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
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <boost/test/unit_test.hpp>


using std::list;
using std::string;
using boost::optional;
using std::shared_ptr;


/* Check that timings are done correctly for multi-reel DCPs with PNG subs */
BOOST_AUTO_TEST_CASE (subtitle_reel_test)
{
	shared_ptr<Film> film = new_test_film2 ("subtitle_reel_test");
	film->set_interop (true);
	shared_ptr<ImageContent> red_a (new ImageContent("test/data/flat_red.png"));
	shared_ptr<ImageContent> red_b (new ImageContent("test/data/flat_red.png"));
	shared_ptr<DCPSubtitleContent> sub_a (new DCPSubtitleContent("test/data/png_subs/subs.xml"));
	shared_ptr<DCPSubtitleContent> sub_b (new DCPSubtitleContent("test/data/png_subs/subs.xml"));

	film->examine_and_add_content (red_a);
	film->examine_and_add_content (red_b);
	film->examine_and_add_content (sub_a);
	film->examine_and_add_content (sub_b);

	BOOST_REQUIRE (!wait_for_jobs());

	red_a->set_position (film, dcpomatic::DCPTime());
	red_a->video->set_length (240);
	sub_a->set_position (film, dcpomatic::DCPTime());
	red_b->set_position (film, dcpomatic::DCPTime::from_seconds(10));
	red_b->video->set_length (240);
	sub_b->set_position (film, dcpomatic::DCPTime::from_seconds(10));

	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	dcp::DCP dcp ("build/test/subtitle_reel_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();

	list<shared_ptr<dcp::Reel> > reels = cpl->reels ();
	BOOST_REQUIRE_EQUAL (reels.size(), 2U);
	list<shared_ptr<dcp::Reel> >::const_iterator i = reels.begin ();
	BOOST_REQUIRE ((*i)->main_subtitle());
	BOOST_REQUIRE ((*i)->main_subtitle()->asset());
	shared_ptr<dcp::InteropSubtitleAsset> A = std::dynamic_pointer_cast<dcp::InteropSubtitleAsset>((*i)->main_subtitle()->asset());
	BOOST_REQUIRE (A);
	++i;
	BOOST_REQUIRE ((*i)->main_subtitle());
	BOOST_REQUIRE ((*i)->main_subtitle()->asset());
	shared_ptr<dcp::InteropSubtitleAsset> B = std::dynamic_pointer_cast<dcp::InteropSubtitleAsset>((*i)->main_subtitle()->asset());
	BOOST_REQUIRE (B);

	BOOST_REQUIRE_EQUAL (A->subtitles().size(), 1U);
	BOOST_REQUIRE_EQUAL (B->subtitles().size(), 1U);

	/* These times should be the same as they are should be offset from the start of the reel */
	BOOST_CHECK (A->subtitles().front()->in() == B->subtitles().front()->in());
}



/** Check that with a SMPTE DCP if we have subtitles in one reel, all reels have a
 *  SubtitleAsset (even if it's empty); SMPTE Bv2.1 section 8.3.1.
 */
BOOST_AUTO_TEST_CASE (subtitle_in_all_reels_test)
{
	shared_ptr<Film> film = new_test_film2 ("subtitle_in_all_reels_test");
	film->set_interop (false);
	film->set_sequence (false);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	for (int i = 0; i < 3; ++i) {
		shared_ptr<Content> video = content_factory("test/data/flat_red.png").front();
		film->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (15 * 24);
		video->set_position (film, dcpomatic::DCPTime::from_seconds(15 * i));
	}
	shared_ptr<Content> subs = content_factory("test/data/15s.srt").front();
	film->examine_and_add_content (subs);
	BOOST_REQUIRE (!wait_for_jobs());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	dcp::DCP dcp ("build/test/subtitle_in_all_reels_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();
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
	shared_ptr<Film> film = new_test_film2 ("closed_captions_in_all_reels_test");
	film->set_interop (false);
	film->set_sequence (false);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);

	for (int i = 0; i < 3; ++i) {
		shared_ptr<Content> video = content_factory("test/data/flat_red.png").front();
		film->examine_and_add_content (video);
		BOOST_REQUIRE (!wait_for_jobs());
		video->video->set_length (15 * 24);
		video->set_position (film, dcpomatic::DCPTime::from_seconds(15 * i));
	}

	shared_ptr<Content> ccap1 = content_factory("test/data/15s.srt").front();
	film->examine_and_add_content (ccap1);
	BOOST_REQUIRE (!wait_for_jobs());
	ccap1->text.front()->set_type (TEXT_CLOSED_CAPTION);
	ccap1->text.front()->set_dcp_track (DCPTextTrack("Test", "de-DE"));

	shared_ptr<Content> ccap2 = content_factory("test/data/15s.srt").front();
	film->examine_and_add_content (ccap2);
	BOOST_REQUIRE (!wait_for_jobs());
	ccap2->text.front()->set_type (TEXT_CLOSED_CAPTION);
	ccap2->text.front()->set_dcp_track (DCPTextTrack("Other", "en-GB"));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	dcp::DCP dcp ("build/test/closed_captions_in_all_reels_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 3U);

	for (auto i: cpl->reels()) {
		BOOST_REQUIRE_EQUAL (i->closed_captions().size(), 2U);
		optional<string> first = i->closed_captions().front()->language();
		optional<string> second = i->closed_captions().back()->language();
		BOOST_REQUIRE (first);
		BOOST_REQUIRE (second);
		BOOST_CHECK (
			(*first == "en-GB" && *second == "de-DE") ||
			(*first == "de-DE" && *second == "en-GB")
			);
	}
}

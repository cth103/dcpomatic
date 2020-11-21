/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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
#include "lib/text_content.h"
#include "lib/string_text_file_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_closed_caption_asset.h>
#include <boost/test/unit_test.hpp>

using std::list;
using boost::shared_ptr;

/** Basic test that Interop closed captions are written */
BOOST_AUTO_TEST_CASE (closed_caption_test1)
{
	shared_ptr<Film> film = new_test_film2 ("closed_caption_test1");
	shared_ptr<StringTextFileContent> content (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	content->only_text()->set_type (TEXT_CLOSED_CAPTION);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Just check to see that there's a CCAP in the CPL: this
	   check could be better!
	*/

	dcp::DCP check (film->dir(film->dcp_name()));
	check.read ();

	BOOST_REQUIRE_EQUAL (check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (check.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (!check.cpls().front()->reels().front()->closed_captions().empty());
}

/** Test multiple closed captions */
BOOST_AUTO_TEST_CASE (closed_caption_test2)
{
	shared_ptr<Film> film = new_test_film2 ("closed_caption_test2");
	shared_ptr<StringTextFileContent> content1 (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (content1);
	shared_ptr<StringTextFileContent> content2 (new StringTextFileContent("test/data/subrip2.srt"));
	film->examine_and_add_content (content2);
	shared_ptr<StringTextFileContent> content3 (new StringTextFileContent("test/data/subrip3.srt"));
	film->examine_and_add_content (content3);
	BOOST_REQUIRE (!wait_for_jobs ());

	content1->only_text()->set_type (TEXT_CLOSED_CAPTION);
	content1->only_text()->set_dcp_track (DCPTextTrack("First track", "fr-FR"));
	content2->only_text()->set_type (TEXT_CLOSED_CAPTION);
	content2->only_text()->set_dcp_track (DCPTextTrack("Second track", "de-DE"));
	content3->only_text()->set_type (TEXT_CLOSED_CAPTION);
	content3->only_text()->set_dcp_track (DCPTextTrack("Third track", "it-IT"));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	dcp::DCP check (film->dir(film->dcp_name()));
	check.read ();

	BOOST_REQUIRE_EQUAL (check.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (check.cpls().front()->reels().size(), 1U);
	list<shared_ptr<dcp::ReelClosedCaptionAsset> > ccaps = check.cpls().front()->reels().front()->closed_captions();
	BOOST_REQUIRE_EQUAL (ccaps.size(), 3U);

	list<shared_ptr<dcp::ReelClosedCaptionAsset> >::const_iterator i = ccaps.begin ();
	BOOST_CHECK_EQUAL ((*i)->annotation_text(), "First track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "fr-FR");
	++i;
	BOOST_CHECK_EQUAL ((*i)->annotation_text(), "Second track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "de-DE");
	++i;
	BOOST_CHECK_EQUAL ((*i)->annotation_text(), "Third track");
	BOOST_REQUIRE (static_cast<bool>((*i)->language()));
	BOOST_CHECK_EQUAL ((*i)->language().get(), "it-IT");
}

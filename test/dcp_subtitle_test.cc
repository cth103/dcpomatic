/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  test/dcp_subtitle_test.cc
 *  @brief Test DCP subtitle content in various ways.
 *  @ingroup specific
 */

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/dcp_content.h"
#include "lib/ratio.h"
#include "lib/dcp_decoder.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/caption_content.h"
#include "lib/content_caption.h"
#include "lib/font.h"
#include "lib/caption_decoder.h"
#include "test.h"
#include <iostream>

using std::cout;
using std::list;
using boost::shared_ptr;
using boost::optional;

optional<ContentTextCaption> stored;

static void
store (ContentTextCaption sub)
{
	if (!stored) {
		stored = sub;
	} else {
		BOOST_FOREACH (dcp::SubtitleString i, sub.subs) {
			stored->subs.push_back (i);
		}
	}
}

/** Test pass-through of a very simple DCP subtitle file */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (false);
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub.xml"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	BOOST_CHECK_EQUAL (content->full_length().get(), DCPTime::from_seconds(2).get());

	content->only_caption()->set_use (true);
	content->only_caption()->set_burn (false);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	check_dcp ("test/data/dcp_subtitle_test", film->dir (film->dcp_name ()));
}

/** Test parsing of a subtitle within an existing DCP */
BOOST_AUTO_TEST_CASE (dcp_subtitle_within_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_within_dcp_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<DCPContent> content (new DCPContent (film, private_data / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<DCPDecoder> decoder (new DCPDecoder (content, film->log(), false));
	decoder->only_caption()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentTextCaption> ();
	while (!decoder->pass() && !stored) {}

	BOOST_REQUIRE (stored);
	BOOST_REQUIRE_EQUAL (stored->subs.size(), 2);
	BOOST_CHECK_EQUAL (stored->subs.front().text(), "Noch mal.");
	BOOST_CHECK_EQUAL (stored->subs.back().text(), "Encore une fois.");
}

/** Test subtitles whose text includes things like &lt;b&gt; */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test2)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub2.xml"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<DCPSubtitleDecoder> decoder (new DCPSubtitleDecoder (content, film->log()));
	decoder->only_caption()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentTextCaption> ();
	while (!decoder->pass ()) {
		if (stored && stored->from() == ContentTime(0)) {
			BOOST_CHECK_EQUAL (stored->subs.front().text(), "&lt;b&gt;Hello world!&lt;/b&gt;");
		}
	}
}

/** Test a failure case */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test3)
{
	shared_ptr<Film> film = new_test_film ("dcp_subtitle_test3");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub3.xml"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<DCPSubtitleDecoder> decoder (new DCPSubtitleDecoder (content, film->log()));
	stored = optional<ContentTextCaption> ();
	while (!decoder->pass ()) {
		decoder->only_caption()->PlainStart.connect (bind (store, _1));
		if (stored && stored->from() == ContentTime::from_seconds(0.08)) {
			list<dcp::SubtitleString> s = stored->subs;
			list<dcp::SubtitleString>::const_iterator i = s.begin ();
			BOOST_CHECK_EQUAL (i->text(), "This");
			++i;
			BOOST_REQUIRE (i != s.end ());
			BOOST_CHECK_EQUAL (i->text(), " is ");
			++i;
			BOOST_REQUIRE (i != s.end ());
			BOOST_CHECK_EQUAL (i->text(), "wrong.");
			++i;
			BOOST_REQUIRE (i == s.end ());
		}
	}
}

/** Check that Interop DCPs aren't made with more than one <LoadFont> (#1273) */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test4)
{
	shared_ptr<Film> film = new_test_film2 ("dcp_subtitle_test4");
	film->set_interop (true);

	shared_ptr<DCPSubtitleContent> content (new DCPSubtitleContent (film, "test/data/dcp_sub3.xml"));
	film->examine_and_add_content (content);
	shared_ptr<DCPSubtitleContent> content2 (new DCPSubtitleContent (film, "test/data/dcp_sub3.xml"));
	film->examine_and_add_content (content2);
	BOOST_REQUIRE (!wait_for_jobs ());

	content->only_caption()->add_font (shared_ptr<Font> (new Font ("font1")));
	content2->only_caption()->add_font (shared_ptr<Font> (new Font ("font2")));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	cxml::Document doc ("DCSubtitle");
	doc.read_file (subtitle_file (film));
	BOOST_REQUIRE_EQUAL (doc.node_children("LoadFont").size(), 1);
}

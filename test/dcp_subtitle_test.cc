/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup feature
 */


#include "lib/content_text.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_decoder.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/text_decoder.h"
#include "test.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/openjpeg_image.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


optional<ContentStringText> stored;


static void
store (ContentStringText sub)
{
	if (!stored) {
		stored = sub;
	} else {
		for (auto i: sub.subs) {
			stored->subs.push_back (i);
		}
	}
}


/** Test pass-through of a very simple DCP subtitle file */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test)
{
	auto film = new_test_film ("dcp_subtitle_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (false);
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub.xml");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	BOOST_CHECK_EQUAL (content->full_length(film).get(), DCPTime::from_seconds(2).get());

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	make_and_verify_dcp (
		film,
	        {
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	/* This test is concerned with the subtitles, so we'll ignore any
	 * differences in sound between the DCP and the reference to avoid test
	 * failures for unrelated reasons.
	 */
	check_dcp("test/data/dcp_subtitle_test", film->dir(film->dcp_name()), true);
}


/** Test parsing of a subtitle within an existing DCP */
BOOST_AUTO_TEST_CASE (dcp_subtitle_within_dcp_test)
{
	auto film = new_test_film ("dcp_subtitle_within_dcp_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto decoder = make_shared<DCPDecoder>(film, content, false, false, shared_ptr<DCPDecoder>());
	decoder->only_text()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentStringText> ();
	while (!decoder->pass() && !stored) {}

	BOOST_REQUIRE (stored);
	BOOST_REQUIRE_EQUAL (stored->subs.size(), 2U);
	BOOST_CHECK_EQUAL (stored->subs.front().text(), "Noch mal.");
	BOOST_CHECK_EQUAL (stored->subs.back().text(), "Encore une fois.");
}

/** Test subtitles whose text includes things like &lt;b&gt; */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test2)
{
	auto film = new_test_film ("dcp_subtitle_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub2.xml");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto decoder = make_shared<DCPSubtitleDecoder>(film, content);
	decoder->only_text()->PlainStart.connect (bind (store, _1));

	stored = optional<ContentStringText> ();
	while (!decoder->pass()) {
		if (stored && stored->from() == ContentTime(0)) {
			/* Text passed around by the player should be unescaped */
			BOOST_CHECK_EQUAL(stored->subs.front().text(), "<b>Hello world!</b>");
		}
	}
}


/** Test a failure case */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test3)
{
	auto film = new_test_film ("dcp_subtitle_test3");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	film->examine_and_add_content (content);
	content->only_text()->set_language(dcp::LanguageTag("de"));
	BOOST_REQUIRE (!wait_for_jobs ());

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	auto decoder = make_shared<DCPSubtitleDecoder>(film, content);
	stored = optional<ContentStringText> ();
	while (!decoder->pass ()) {
		decoder->only_text()->PlainStart.connect (bind (store, _1));
		if (stored && stored->from() == ContentTime::from_seconds(0.08)) {
			auto s = stored->subs;
			auto i = s.begin ();
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
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	auto content2 = make_shared<DCPSubtitleContent>("test/data/dcp_sub3.xml");
	auto film = new_test_film2 ("dcp_subtitle_test4", {content, content2});
	film->set_interop (true);

	content->only_text()->add_font(make_shared<Font>("font1"));
	content2->only_text()->add_font(make_shared<Font>("font2"));
	content->only_text()->set_language(dcp::LanguageTag("de"));
	content2->only_text()->set_language(dcp::LanguageTag("de"));

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cxml::Document doc ("DCSubtitle");
	doc.read_file (subtitle_file (film));
	BOOST_REQUIRE_EQUAL (doc.node_children("LoadFont").size(), 1U);
}


static
void
check_font_tags (vector<cxml::NodePtr> nodes)
{
	for (auto i: nodes) {
		if (i->name() == "Font") {
			BOOST_CHECK (!i->optional_string_attribute("Id") || i->string_attribute("Id") != "");
		}
		check_font_tags (i->node_children());
	}
}


/** Check that imported <LoadFont> tags with empty IDs (or corresponding Font tags with empty IDs)
 *  are not passed through into the DCP.
 */
BOOST_AUTO_TEST_CASE (dcp_subtitle_test5)
{
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub6.xml");
	auto film = new_test_film2 ("dcp_subtitle_test5", {content});
	film->set_interop (true);
	content->only_text()->set_language(dcp::LanguageTag("de"));

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	cxml::Document doc ("DCSubtitle");
	doc.read_file (subtitle_file(film));
	BOOST_REQUIRE_EQUAL (doc.node_children("LoadFont").size(), 1U);
	BOOST_CHECK (doc.node_children("LoadFont").front()->string_attribute("Id") != "");

	check_font_tags (doc.node_children());
}


/** Check that fonts specified in the DoM content are used in the output and not ignored (#2074) */
BOOST_AUTO_TEST_CASE (test_font_override)
{
	auto content = make_shared<DCPSubtitleContent>("test/data/dcp_sub4.xml");
	auto film = new_test_film2("test_font_override", {content});
	film->set_interop(true);
	content->only_text()->set_language(dcp::LanguageTag("de"));

	BOOST_REQUIRE_EQUAL(content->text.size(), 1U);
	auto font = content->text.front()->get_font("0_theFontId");
	BOOST_REQUIRE(font);
	font->set_file("test/data/Inconsolata-VF.ttf");

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });
	check_file (subtitle_file(film).parent_path() / "font_0.ttf", "test/data/Inconsolata-VF.ttf");
}


BOOST_AUTO_TEST_CASE(entity_from_dcp_source)
{
	std::ofstream source_xml("build/test/entity_from_dcp_source.xml");
	source_xml
		<< "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<SubtitleReel xmlns=\"http://www.smpte-ra.org/schemas/428-7/2010/DCST\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
		<< "<Id>urn:uuid:9c0a0a67-ffd8-4c65-8b5a-c6be3ef182c5</Id>\n"
		<< "<ContentTitleText>DCP</ContentTitleText>\n"
		<< "<IssueDate>2022-11-30T18:13:56.000+01:00</IssueDate>\n"
		<< "<ReelNumber>1</ReelNumber>\n"
		<< "<EditRate>24 1</EditRate>\n"
		<< "<TimeCodeRate>24</TimeCodeRate>\n"
		<< "<StartTime>00:00:00:00</StartTime>\n"
		<< "<LoadFont ID=\"font\">urn:uuid:899e5c59-50f6-467b-985b-8282c020e1ee</LoadFont>\n"
		<< "<SubtitleList>\n"
		<< "<Font AspectAdjust=\"1.0\" Color=\"FFFFFFFF\" Effect=\"none\" EffectColor=\"FF000000\" ID=\"font\" Italic=\"no\" Script=\"normal\" Size=\"48\" Underline=\"no\" Weight=\"normal\">\n"
		<< "<Subtitle SpotNumber=\"1\" TimeIn=\"00:00:00:00\" TimeOut=\"00:00:10:00\" FadeUpTime=\"00:00:00:00\" FadeDownTime=\"00:00:00:00\">\n"
		<< "<Text Valign=\"top\" Vposition=\"82.7273\">Hello &amp; world</Text>\n"
		<< "</Subtitle>\n"
		<< "</Font>\n"
		<< "</SubtitleList>\n"
		<< "</SubtitleReel>\n";
	source_xml.close();

	auto content = make_shared<DCPSubtitleContent>("build/test/entity_from_dcp_source.xml");
	auto film = new_test_film2("entity_from_dcp_source", { content });
	film->set_interop(false);
	content->only_text()->set_use(true);
	content->only_text()->set_burn(false);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
		});

	dcp::SMPTESubtitleAsset check(dcp_file(film, "sub_"));
	auto subs = check.subtitles();
	BOOST_REQUIRE_EQUAL(subs.size(), 1U);
	auto sub = std::dynamic_pointer_cast<const dcp::SubtitleString>(subs[0]);
	BOOST_REQUIRE(sub);
	/* libdcp::SubtitleAsset gets the text from the XML with get_content(), which
	 * resolves the 5 predefined entities & " < > ' so we shouldn't see any
	 * entity here.
	 */
	BOOST_CHECK_EQUAL(sub->text(), "Hello & world");

	/* It should be escaped in the raw XML though */
	BOOST_REQUIRE(static_cast<bool>(check.raw_xml()));
	BOOST_CHECK(check.raw_xml()->find("Hello &amp; world") != std::string::npos);

	/* Remake with burn */
	content->only_text()->set_burn(true);
	boost::filesystem::remove_all(film->dir(film->dcp_name()));
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
		});

	dcp::MonoPictureAsset burnt(dcp_file(film, "j2c_"));
	auto frame = burnt.start_read()->get_frame(12)->xyz_image();
	auto const size = frame->size();
	int max_X = 0;
	for (auto y = 0; y < size.height; ++y) {
		for (auto x = 0; x < size.width; ++x) {
			max_X = std::max(frame->data(0)[x + y * size.width], max_X);
		}
	}

	/* Check that the subtitle got rendered to the image; if the escaping of the & is wrong Pango
	 * will throw errors and nothing will be rendered.
	 */
	BOOST_CHECK(max_X > 100);
}


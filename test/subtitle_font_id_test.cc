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


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/player.h"
#include "lib/text_content.h"
#include "lib/util.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(full_dcp_subtitle_font_id_test)
{
	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film("full_dcp_subtitle_font_id_test", { dcp });

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "0_theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(dcp_subtitle_font_id_test)
{
	auto subs = content_factory(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV" / "8b48f6ae-c74b-4b80-b994-a8236bbbad74_sub.mxf");
	auto film = new_test_film("dcp_subtitle_font_id_test", subs);

	auto content = film->content();
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	auto text = content[0]->only_text();
	BOOST_REQUIRE(text);

	BOOST_REQUIRE_EQUAL(text->fonts().size(), 1U);
	auto font = text->fonts().front();
	BOOST_CHECK_EQUAL(font->id(), "0_theFontId");
	BOOST_REQUIRE(font->data());
	BOOST_CHECK_EQUAL(font->data()->size(), 367112);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_interop_dcp)
{
	auto dcp = make_shared<DCPContent>("test/data/Iopsubs_FTR-1_F_XX-XX_MOS_2K_20220710_IOP_OV");
	auto film = new_test_film("make_dcp_with_subs_from_interop_dcp", { dcp });
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		}
	);
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_smpte_dcp)
{
	Cleanup cl;

	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film("make_dcp_with_subs_from_smpte_dcp", { dcp }, &cl);
	dcp->text.front()->set_use(true);
	make_and_verify_dcp(film);

	cl.run();
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_from_mkv)
{
	auto subs = content_factory(TestPaths::private_data() / "clapperboard_with_subs.mkv");
	auto film = new_test_film("make_dcp_with_subs_from_mkv", subs);
	subs[0]->text.front()->set_use(true);
	subs[0]->text.front()->set_language(dcp::LanguageTag("en"));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K });
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_without_font_tag)
{
	auto subs = content_factory("test/data/no_font.xml");
	auto film = new_test_film("make_dcp_with_subs_without_font_tag", { subs });
	subs[0]->text.front()->set_use(true);
	subs[0]->text.front()->set_language(dcp::LanguageTag("de"));
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	auto check_file = subtitle_file(film);
	dcp::SMPTESubtitleAsset check_asset(check_file);
	BOOST_CHECK_EQUAL(check_asset.load_font_nodes().size(), 1U);
	auto check_font_data = check_asset.font_data();
	BOOST_CHECK_EQUAL(check_font_data.size(), 1U);
	BOOST_CHECK(check_font_data.begin()->second == dcp::ArrayData(default_font_file()));
}


BOOST_AUTO_TEST_CASE(make_dcp_with_subs_in_dcp_without_font_tag)
{
	/* Make a DCP with some subs in */
	auto source_subs = content_factory("test/data/short.srt");
	auto source = new_test_film("make_dcp_with_subs_in_dcp_without_font_tag_source", { source_subs });
	source->set_interop(true);
	source_subs[0]->only_text()->set_language(dcp::LanguageTag("de"));
	make_and_verify_dcp(
		source,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::INVALID_STANDARD
		});

	/* Find the ID of the subs */
	dcp::DCP source_dcp(source->dir(source->dcp_name()));
	source_dcp.read();
	BOOST_REQUIRE(!source_dcp.cpls().empty());
	BOOST_REQUIRE(!source_dcp.cpls()[0]->reels().empty());
	BOOST_REQUIRE(source_dcp.cpls()[0]->reels()[0]->main_subtitle());
	auto const id = source_dcp.cpls()[0]->reels()[0]->main_subtitle()->asset()->id();

	/* Graft in some bad subs with no <Font> tag */
	auto source_subtitle_file = subtitle_file(source);
#if BOOST_VERSION >= 107400
	boost::filesystem::copy_file("test/data/no_font.xml", source_subtitle_file, boost::filesystem::copy_options::overwrite_existing);
#else
	boost::filesystem::copy_file("test/data/no_font.xml", source_subtitle_file, boost::filesystem::copy_option::overwrite_if_exists);
#endif

	/* Fix the <Id> tag */
	{
		Editor editor(source_subtitle_file);
		editor.replace("4dd8ee05-5986-4c67-a6f8-bbeac62e21db", id);
	}

	/* Now make a project which imports that DCP and makes another DCP from it */
	auto dcp_content = make_shared<DCPContent>(source->dir(source->dcp_name()));
	auto film = new_test_film("make_dcp_with_subs_without_font_tag", { dcp_content });
	BOOST_REQUIRE(!dcp_content->text.empty());
	dcp_content->text.front()->set_use(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	auto check_file = subtitle_file(film);
	dcp::SMPTESubtitleAsset check_asset(check_file);
	BOOST_CHECK_EQUAL(check_asset.load_font_nodes().size(), 1U);
	auto check_font_data = check_asset.font_data();
	BOOST_CHECK_EQUAL(check_font_data.size(), 1U);
	BOOST_CHECK(check_font_data.begin()->second == dcp::ArrayData(default_font_file()));
}


BOOST_AUTO_TEST_CASE(filler_subtitle_reels_have_load_font_tags)
{
	auto const name = boost::unit_test::framework::current_test_case().full_name();

	auto subs = content_factory("test/data/short.srt")[0];
	auto video1 = content_factory("test/data/flat_red.png")[0];
	auto video2 = content_factory("test/data/flat_red.png")[0];

	auto film = new_test_film(name, { video1, video2, subs });
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
}


BOOST_AUTO_TEST_CASE(subtitle_with_no_font_test)
{
	auto const name_base = boost::unit_test::framework::current_test_case().full_name();

	auto video1 = content_factory("test/data/flat_red.png")[0];
	auto video2 = content_factory("test/data/flat_red.png")[0];
	auto subs = content_factory("test/data/short.srt")[0];

	auto bad_film = new_test_film(name_base + "_bad", { video1, video2, subs });
	bad_film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	video2->set_position(bad_film, video1->end(bad_film));
	subs->set_position(bad_film, video1->end(bad_film));
	subs->text[0]->add_font(make_shared<dcpomatic::Font>("foo", "test/data/LiberationSans-Regular.ttf"));

	make_and_verify_dcp(
		bad_film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	/* When this test was written, this DCP would have one reel whose subtitles had <LoadFont>s
	 * but the subtitles specified no particular font.  This triggers bug #2649, which this test
	 * is intended to trigger.  First, make sure that the DCP has the required characteristics,
	 * to guard against a case where for some reason the DCP here is different enough that it
	 * doesn't trigger the bug.
	 */
	dcp::DCP check(bad_film->dir(bad_film->dcp_name()));
	check.read();
	BOOST_REQUIRE_EQUAL(check.cpls().size(), 1U);
	auto cpl = check.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 2U);
	auto check_subs_reel = cpl->reels()[0]->main_subtitle();
	BOOST_REQUIRE(check_subs_reel);
	auto check_subs = check_subs_reel->asset();
	BOOST_REQUIRE(check_subs);

	BOOST_CHECK_EQUAL(check_subs->font_data().size(), 1U);
	BOOST_REQUIRE_EQUAL(check_subs->subtitles().size(), 1U);
	BOOST_CHECK(!std::dynamic_pointer_cast<const dcp::SubtitleString>(check_subs->subtitles()[0])->font().has_value());

	auto check_film = new_test_film(name_base + "_check", { make_shared<DCPContent>(bad_film->dir(bad_film->dcp_name())) });
	make_and_verify_dcp(check_film);
}


BOOST_AUTO_TEST_CASE(load_dcp_with_empty_font_id_test)
{
	auto dcp = std::make_shared<DCPContent>(TestPaths::private_data() / "kr_vf");
	auto film = new_test_film("load_dcp_with_empty_font_id_test", { dcp });
}


BOOST_AUTO_TEST_CASE(use_first_loadfont_as_default)
{
	auto dcp = std::make_shared<DCPContent>("test/data/use_default_font");
	auto film = new_test_film("use_first_loadfont_as_default", { dcp });
	dcp->only_text()->set_use(true);
	dcp->only_text()->set_language(dcp::LanguageTag("de"));
	make_and_verify_dcp(
		film,
		{ dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME }
		);

	dcp::DCP test(film->dir(film->dcp_name()));
	test.read();
	BOOST_REQUIRE(!test.cpls().empty());
	auto cpl = test.cpls()[0];
	BOOST_REQUIRE(!cpl->reels().empty());
	auto reel = cpl->reels()[0];
	BOOST_REQUIRE(reel->main_subtitle()->asset());
	auto subtitle = std::dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(reel->main_subtitle()->asset());
	BOOST_REQUIRE_EQUAL(subtitle->font_data().size(), 1U);
	BOOST_CHECK(subtitle->font_data().begin()->second == dcp::ArrayData("test/data/Inconsolata-VF.ttf"));
}


BOOST_AUTO_TEST_CASE(no_error_with_ccap_that_mentions_no_font)
{
	auto dcp = make_shared<DCPContent>("test/data/ccap_only");
	auto film = new_test_film("no_error_with_ccap_that_mentions_no_font", { dcp });
	auto player = Player(film, film->playlist());
	while (!player.pass()) {}
}


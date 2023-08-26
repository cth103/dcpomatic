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


/** @file  test/burnt_subtitle_test.cc
 *  @brief Test the burning of subtitles into the DCP.
 *  @ingroup feature
 */


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/log_entry.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_mono_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::map;
using std::string;
using namespace dcpomatic;


/** Build a small DCP with no picture and a single subtitle overlaid onto it from a SubRip file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_subrip)
{
	auto film = new_test_film ("burnt_subtitle_test_subrip");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = content_factory("test/data/subrip2.srt")[0];
	content->text[0]->set_use(true);
	content->text[0]->set_burn(true);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp(
		film,
		{ dcp::VerificationNote::Code::MISSING_CPL_METADATA }
		);

#ifdef DCPOMATIC_WINDOWS
	check_dcp("test/data/windows/burnt_subtitle_test_subrip", film);
#else
	check_dcp("test/data/burnt_subtitle_test_subrip", film);
#endif
}

/** Build a small DCP with no picture and a single subtitle overlaid onto it from a DCP XML file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_dcp)
{
	auto film = new_test_film ("burnt_subtitle_test_dcp");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	auto content = content_factory("test/data/dcp_sub.xml")[0];
	content->text[0]->set_use(true);
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	check_dcp("test/data/burnt_subtitle_test_dcp", film);
}

/** Burn some subtitles into an existing DCP to check the colour conversion */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_onto_dcp)
{
	auto film = new_test_film ("burnt_subtitle_test_onto_dcp");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->examine_and_add_content(content_factory("test/data/flat_black.png")[0]);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (film);

	Config::instance()->set_log_types (Config::instance()->log_types() | LogEntry::TYPE_DEBUG_ENCODE);
	auto film2 = new_test_film ("burnt_subtitle_test_onto_dcp2");
	film2->set_container (Ratio::from_id ("185"));
	film2->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film2->set_name ("frobozz");
	auto background_dcp = make_shared<DCPContent>(film->dir(film->dcp_name()));
	film2->examine_and_add_content (background_dcp);
	auto sub = content_factory("test/data/subrip2.srt")[0];
	sub->text[0]->set_burn(true);
	sub->text[0]->set_effect(dcp::Effect::BORDER);
	film2->examine_and_add_content (sub);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (film2);

	BOOST_CHECK (background_dcp->position() == DCPTime());
	BOOST_CHECK (sub->position() == DCPTime());

	dcp::DCP dcp (film2->dir (film2->dcp_name ()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (dcp.cpls().front()->reels().front()->main_picture());
	BOOST_REQUIRE (dcp.cpls().front()->reels().front()->main_picture()->asset());
	auto pic = dynamic_pointer_cast<dcp::ReelMonoPictureAsset> (
		dcp.cpls().front()->reels().front()->main_picture()
		)->mono_asset();
	BOOST_REQUIRE (pic);
	auto frame = pic->start_read()->get_frame(12);
	auto xyz = frame->xyz_image ();
	BOOST_CHECK_EQUAL (xyz->size().width, 1998);
	BOOST_CHECK_EQUAL (xyz->size().height, 1080);

#ifdef DCPOMATIC_WINDOWS
	check_dcp("test/data/windows/burnt_subtitle_test_onto_dcp2", film2);
#else
	check_dcp("test/data/burnt_subtitle_test_onto_dcp2", film2);
#endif
}



/** Check positioning of some burnt subtitles from XML files */
BOOST_AUTO_TEST_CASE(burnt_subtitle_test_position)
{
	auto check = [](string alignment)
	{
		auto const name = String::compose("burnt_subtitle_test_position_%1", alignment);
		auto subs = content_factory(String::compose("test/data/burn_%1.xml", alignment));
		auto film = new_test_film2(name, subs);
		subs[0]->text[0]->set_use(true);
		subs[0]->text[0]->set_burn(true);
		make_and_verify_dcp(
			film,
			{
				dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
				dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME,
				dcp::VerificationNote::Code::MISSING_CPL_METADATA
			});

		check_dcp(String::compose("test/data/%1", name), film);
	};

	/* Should have a baseline 216 pixels from the top (0.2 * 1080) */
	check("top");
	/* Should have a baseline 756 pixels from the top ((0.5 + 0.2) * 1080) */
	check("center");
	/* Should have a baseline 864 pixels from the top ((1 - 0.2) * 1080) */
	check("bottom");
}

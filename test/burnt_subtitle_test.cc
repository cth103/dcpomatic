/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup specific
 */

#include "lib/plain_text_content.h"
#include "lib/dcp_text_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/text_content.h"
#include "lib/dcp_content.h"
#include "lib/content_factory.h"
#include "lib/config.h"
#include "lib/log_entry.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/j2k.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_mono_picture_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::cout;
using std::map;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Build a small DCP with no picture and a single subtitle overlaid onto it from a SubRip file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_subrip)
{
	shared_ptr<Film> film = new_test_film ("burnt_subtitle_test_subrip");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<StringText> content (new StringText (film, "test/data/subrip2.srt"));
	content->subtitle->set_use (true);
	content->subtitle->set_burn (true);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/burnt_subtitle_test_subrip", film->dir (film->dcp_name ()));
}

/** Build a small DCP with no picture and a single subtitle overlaid onto it from a DCP XML file */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_dcp)
{
	shared_ptr<Film> film = new_test_film ("burnt_subtitle_test_dcp");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<DCPTextContent> content (new DCPTextContent (film, "test/data/dcp_sub.xml"));
	content->subtitle->set_use (true);
	film->examine_and_add_content (content);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/burnt_subtitle_test_dcp", film->dir (film->dcp_name ()));
}

/** Burn some subtitles into an existing DCP to check the colour conversion */
BOOST_AUTO_TEST_CASE (burnt_subtitle_test_onto_dcp)
{
	shared_ptr<Film> film = new_test_film ("burnt_subtitle_test_onto_dcp");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->examine_and_add_content (content_factory(film, "test/data/flat_black.png").front());
	BOOST_REQUIRE (!wait_for_jobs());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	Config::instance()->set_log_types (Config::instance()->log_types() | LogEntry::TYPE_DEBUG_ENCODE);
	shared_ptr<Film> film2 = new_test_film ("burnt_subtitle_test_onto_dcp2");
	film2->set_container (Ratio::from_id ("185"));
	film2->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film2->set_name ("frobozz");
	shared_ptr<DCPContent> background_dcp (new DCPContent(film2, film->dir(film->dcp_name())));
	film2->examine_and_add_content (background_dcp);
	shared_ptr<StringText> sub = dynamic_pointer_cast<StringText> (
		content_factory(film2, "test/data/subrip2.srt").front()
		);
	sub->subtitle->set_burn (true);
	sub->subtitle->set_outline (true);
	film2->examine_and_add_content (sub);
	BOOST_REQUIRE (!wait_for_jobs());
	film2->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_CHECK (background_dcp->position() == DCPTime());
	BOOST_CHECK (sub->position() == DCPTime());

	dcp::DCP dcp (film2->dir (film2->dcp_name ()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1);
	BOOST_REQUIRE_EQUAL (dcp.cpls().front()->reels().size(), 1);
	BOOST_REQUIRE (dcp.cpls().front()->reels().front()->main_picture());
	BOOST_REQUIRE (dcp.cpls().front()->reels().front()->main_picture()->asset());
	shared_ptr<const dcp::MonoPictureAsset> pic = dynamic_pointer_cast<dcp::ReelMonoPictureAsset> (
		dcp.cpls().front()->reels().front()->main_picture()
		)->mono_asset();
	BOOST_REQUIRE (pic);
	shared_ptr<const dcp::MonoPictureFrame> frame = pic->start_read()->get_frame (12);
	shared_ptr<dcp::OpenJPEGImage> xyz = frame->xyz_image ();
	BOOST_CHECK_EQUAL (xyz->size().width, 1998);
	BOOST_CHECK_EQUAL (xyz->size().height, 1080);

	check_dcp ("test/data/burnt_subtitle_test_onto_dcp", film->dir(film->dcp_name()));
}

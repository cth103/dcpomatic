/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/text_subtitle_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/subtitle_content.h"
#include "lib/dcp_content_type.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/* Check that ReelNumber is setup correctly when making multi-reel subtitled DCPs */
BOOST_AUTO_TEST_CASE (subtitle_reel_number_test)
{
	shared_ptr<Film> film = new_test_film ("subtitle_reel_number_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, "test/data/subrip5.srt"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);
	film->set_reel_type (REELTYPE_BY_LENGTH);
	film->set_interop (true);
	film->set_reel_length (1024 * 1024 * 512);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	dcp::DCP dcp ("build/test/subtitle_reel_number_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 6);

	int n = 1;
	BOOST_FOREACH (shared_ptr<dcp::Reel> i, cpl->reels()) {
		if (i->main_subtitle()) {
			shared_ptr<dcp::InteropSubtitleAsset> ass = dynamic_pointer_cast<dcp::InteropSubtitleAsset>(i->main_subtitle()->asset());
			BOOST_REQUIRE (ass);
			BOOST_CHECK_EQUAL (ass->reel_number(), dcp::raw_convert<string>(n));
			++n;
		}
	}
}

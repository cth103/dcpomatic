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

#include "lib/string_text_file_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
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
using std::shared_ptr;
using std::dynamic_pointer_cast;

/* Check that ReelNumber is setup correctly when making multi-reel subtitled DCPs */
BOOST_AUTO_TEST_CASE (subtitle_reel_number_test)
{
	shared_ptr<Film> film = new_test_film ("subtitle_reel_number_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	shared_ptr<StringTextFileContent> content (new StringTextFileContent("test/data/subrip5.srt"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	film->set_reel_type (ReelType::BY_LENGTH);
	film->set_interop (true);
	film->set_reel_length (1024 * 1024 * 512);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	dcp::DCP dcp ("build/test/subtitle_reel_number_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 6U);

	int n = 1;
	for (auto i: cpl->reels()) {
		if (i->main_subtitle()) {
			auto ass = dynamic_pointer_cast<dcp::InteropSubtitleAsset>(i->main_subtitle()->asset());
			BOOST_REQUIRE (ass);
			BOOST_CHECK_EQUAL (ass->reel_number(), dcp::raw_convert<string>(n));
			++n;
		}
	}
}

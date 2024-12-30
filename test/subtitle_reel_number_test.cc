/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/interop_text_asset.h>
#include <dcp/reel_text_asset.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::string;


/* Check that ReelNumber is setup correctly when making multi-reel subtitled DCPs */
BOOST_AUTO_TEST_CASE (subtitle_reel_number_test)
{
	Cleanup cl;

	auto content = make_shared<StringTextFileContent>("test/data/subrip5.srt");
	auto film = new_test_film("subtitle_reel_number_test", { content }, &cl);
	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	content->only_text()->set_language(dcp::LanguageTag("de"));
	film->set_reel_type (ReelType::BY_LENGTH);
	film->set_interop (true);
	film->set_reel_length (1024 * 1024 * 512);
	film->set_video_bit_rate(VideoEncoding::JPEG2000, 100000000);
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_STANDARD});

	dcp::DCP dcp ("build/test/subtitle_reel_number_test/" + film->dcp_name());
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 6U);

	int n = 1;
	for (auto i: cpl->reels()) {
		if (i->main_subtitle()) {
			auto ass = dynamic_pointer_cast<dcp::InteropTextAsset>(i->main_subtitle()->asset());
			BOOST_REQUIRE (ass);
			BOOST_CHECK_EQUAL (ass->reel_number(), fmt::to_string(n));
			++n;
		}
	}

	cl.run();
}

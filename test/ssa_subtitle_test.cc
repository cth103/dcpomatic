/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/ssa_subtitle_test.cc
 *  @brief Test use of SSA subtitle files.
 *  @ingroup feature
 */


#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/ratio.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "test.h"
#include <dcp/equality_options.h>
#include <dcp/interop_subtitle_asset.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>


using std::list;
using std::make_shared;
using std::string;


/** Make a DCP with subs from a .ssa file */
BOOST_AUTO_TEST_CASE (ssa_subtitle_test1)
{
	Cleanup cl;

	auto film = new_test_film("ssa_subtitle_test1", {}, &cl);

	film->set_container (Ratio::from_id ("185"));
	film->set_name ("frobozz");
	film->set_interop (true);
	auto content = make_shared<StringTextFileContent>(TestPaths::private_data() / "DKH_UT_EN20160601def.ssa");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	content->only_text()->set_use (true);
	content->only_text()->set_burn (false);
	content->only_text()->set_language(dcp::LanguageTag("de"));

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	auto ref = make_shared<dcp::InteropSubtitleAsset>(TestPaths::private_data() / "DKH_UT_EN20160601def.xml");
	auto check = make_shared<dcp::InteropSubtitleAsset>(subtitle_file(film));

	dcp::EqualityOptions options;
	options.max_subtitle_vertical_position_error = 0.1;
	BOOST_CHECK(ref->equals(check, options, [](dcp::NoteType t, string n) {
		if (t == dcp::NoteType::ERROR) {
			std::cerr << n << "\n";
		}
	}));

	cl.run ();
}

/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "test.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_smpte_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE(test_text_entry_point)
{
	auto const path = boost::filesystem::path("build/test/test_text_entry_point");
	boost::filesystem::remove_all(path);
	boost::filesystem::create_directories(path);

	/* Make a "bad" DCP with a non-zero text entry point */
	dcp::DCP bad_dcp(path / "dcp");
	auto sub = make_shared<dcp::SMPTESubtitleAsset>();
	sub->write(path / "dcp" / "subs.mxf");
	auto reel_sub = make_shared<dcp::ReelSMPTESubtitleAsset>(sub, dcp::Fraction{24, 1}, 42, 6);
	auto reel = make_shared<dcp::Reel>();
	reel->add(reel_sub);

	auto cpl = make_shared<dcp::CPL>("foo", dcp::ContentKind::FEATURE, dcp::Standard::SMPTE);
	bad_dcp.add(cpl);
	cpl->add(reel);

	bad_dcp.write_xml();

	/* Make a film and add the bad DCP, so that the examiner spots the problem */
	auto dcp_content = make_shared<DCPContent>(path / "dcp");
	auto film = new_test_film("test_text_entry_point/film", { dcp_content });
	film->write_metadata();

	/* Reload the film to check that the examiner's output is saved and recovered */
	auto film2 = make_shared<Film>(path / "film");
	film2->read_metadata();

	string why_not;
	BOOST_CHECK(!dcp_content->can_reference_text(film2, TextType::OPEN_SUBTITLE, why_not));
	BOOST_CHECK_EQUAL(why_not, "one of its subtitle reels has a non-zero entry point so it must be re-written.");
}


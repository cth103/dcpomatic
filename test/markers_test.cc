/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/markers_test
 *  @brief Test SMPTE markers.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel_markers_asset.h>
#include <dcp/reel.h>
#include <boost/test/unit_test.hpp>


using std::string;
using boost::optional;
using std::shared_ptr;


/** Check that FFOC and LFOC are automatically added if not specified */
BOOST_AUTO_TEST_CASE (automatic_ffoc_lfoc_markers_test1)
{
	string const name = "automatic_ffoc_lfoc_markers_test1";
	auto film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/flat_red.png")[0]);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_interop (false);
	make_and_verify_dcp (film);

	dcp::DCP dcp (String::compose("build/test/%1/%2", name, film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	auto markers = reel->main_markers();
	BOOST_REQUIRE (markers);

	auto ffoc = markers->get (dcp::Marker::FFOC);
	BOOST_REQUIRE (ffoc);
	BOOST_CHECK (*ffoc == dcp::Time(0, 0, 0, 1, 24));
	auto lfoc = markers->get (dcp::Marker::LFOC);
	BOOST_REQUIRE (lfoc);
	BOOST_CHECK (*lfoc == dcp::Time(0, 0, 9, 23, 24));
}


/** Check that FFOC and LFOC are not overridden if they are specified */
BOOST_AUTO_TEST_CASE (automatic_ffoc_lfoc_markers_test2)
{
	string const name = "automatic_ffoc_lfoc_markers_test2";
	auto film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/flat_red.png")[0]);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_interop (false);
	film->set_marker (dcp::Marker::FFOC, dcpomatic::DCPTime::from_seconds(1));
	film->set_marker (dcp::Marker::LFOC, dcpomatic::DCPTime::from_seconds(9));
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::INCORRECT_FFOC,
			dcp::VerificationNote::Code::INCORRECT_LFOC
		});

	dcp::DCP dcp (String::compose("build/test/%1/%2", name, film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	auto markers = reel->main_markers();
	BOOST_REQUIRE (markers);

	auto ffoc = markers->get (dcp::Marker::FFOC);
	BOOST_REQUIRE (ffoc);
	BOOST_CHECK (*ffoc == dcp::Time (0, 0, 1, 0, 24));
	auto lfoc = markers->get (dcp::Marker::LFOC);
	BOOST_REQUIRE (lfoc);
	BOOST_CHECK (*lfoc == dcp::Time(0, 0, 9, 0, 24));
}



BOOST_AUTO_TEST_CASE(markers_correct_with_reels)
{
	string const name = "markers_correct_with_reels";
	auto content1 = content_factory("test/data/flat_red.png")[0];
	auto content2 = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film2(name, { content1, content2});

	film->set_interop(false);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	make_and_verify_dcp(film);

	dcp::DCP dcp(String::compose("build/test/%1/%2", name, film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL(cpl->reels().size(), 2U);

	auto markers1 = cpl->reels()[0]->main_markers();
	BOOST_REQUIRE(markers1);
	auto ffoc = markers1->get(dcp::Marker::FFOC);
	BOOST_REQUIRE(ffoc);
	BOOST_CHECK(*ffoc == dcp::Time(0, 0, 0, 1, 24));
	auto no_lfoc = markers1->get(dcp::Marker::LFOC);
	BOOST_CHECK(!no_lfoc);

	auto markers2 = cpl->reels()[1]->main_markers();
	BOOST_REQUIRE(markers2);
	auto no_ffoc = markers2->get(dcp::Marker::FFOC);
	BOOST_REQUIRE(!no_ffoc);
	auto lfoc = markers2->get(dcp::Marker::LFOC);
	BOOST_REQUIRE(lfoc);
	BOOST_CHECK(*lfoc == dcp::Time(0, 0, 9, 23, 24));
}


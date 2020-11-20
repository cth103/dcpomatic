/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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
using boost::shared_ptr;


/** Check that FFOC and LFOC are automatically added if not specified */
BOOST_AUTO_TEST_CASE (automatic_ffoc_lfoc_markers_test1)
{
	string const name = "automatic_ffoc_lfoc_markers_test1";
	shared_ptr<Film> film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/flat_red.png").front());
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_interop (false);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	dcp::DCP dcp (String::compose("build/test/%1/%2", name, film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1);
	shared_ptr<dcp::Reel> reel = cpl->reels().front();
	shared_ptr<dcp::ReelMarkersAsset> markers = reel->main_markers();
	BOOST_REQUIRE (markers);

	optional<dcp::Time> ffoc = markers->get (dcp::Marker::FFOC);
	BOOST_REQUIRE (ffoc);
	BOOST_CHECK (*ffoc == dcp::Time (0, 0, 0, 0, 24));
	optional<dcp::Time> lfoc = markers->get (dcp::Marker::LFOC);
	BOOST_REQUIRE (lfoc);
	BOOST_CHECK (*lfoc == dcp::Time(0, 0, 9, 23, 24));
}


/** Check that FFOC and LFOC are not overridden if they are specified */
BOOST_AUTO_TEST_CASE (automatic_ffoc_lfoc_markers_test2)
{
	string const name = "automatic_ffoc_lfoc_markers_test2";
	shared_ptr<Film> film = new_test_film2 (name);
	film->examine_and_add_content (content_factory("test/data/flat_red.png").front());
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_interop (false);
	film->set_marker (dcp::Marker::FFOC, dcpomatic::DCPTime::from_seconds(1));
	film->set_marker (dcp::Marker::LFOC, dcpomatic::DCPTime::from_seconds(9));
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	dcp::DCP dcp (String::compose("build/test/%1/%2", name, film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1);
	shared_ptr<dcp::CPL> cpl = dcp.cpls().front();
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1);
	shared_ptr<dcp::Reel> reel = cpl->reels().front();
	shared_ptr<dcp::ReelMarkersAsset> markers = reel->main_markers();
	BOOST_REQUIRE (markers);

	optional<dcp::Time> ffoc = markers->get (dcp::Marker::FFOC);
	BOOST_REQUIRE (ffoc);
	BOOST_CHECK (*ffoc == dcp::Time (0, 0, 1, 0, 24));
	optional<dcp::Time> lfoc = markers->get (dcp::Marker::LFOC);
	BOOST_REQUIRE (lfoc);
	BOOST_CHECK (*lfoc == dcp::Time(0, 0, 9, 0, 24));
}


/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  test/film_metadata_test.cc
 *  @brief Test some basic reading/writing of film metadata.
 */

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "test.h"

using std::string;
using std::list;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (film_metadata_test)
{
	shared_ptr<Film> film = new_test_film ("film_metadata_test");
	boost::filesystem::path dir = test_film_dir ("film_metadata_test");

	film->_isdcf_date = boost::gregorian::from_undelimited_string ("20130211");
	BOOST_CHECK (film->container() == Ratio::from_id ("185"));
	BOOST_CHECK (film->dcp_content_type() == 0);

	film->set_name ("fred");
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Short"));
	film->set_container (Ratio::from_id ("185"));
	film->set_j2k_bandwidth (200000000);
	film->write_metadata ();

	list<string> ignore;
	ignore.push_back ("Key");
	check_xml ("test/data/metadata.xml.ref", dir.string() + "/metadata.xml", ignore);

	shared_ptr<Film> g (new Film (dir));
	g->read_metadata ();

	BOOST_CHECK_EQUAL (g->name(), "fred");
	BOOST_CHECK_EQUAL (g->dcp_content_type(), DCPContentType::from_pretty_name ("Short"));
	BOOST_CHECK_EQUAL (g->container(), Ratio::from_id ("185"));
	
	g->write_metadata ();
	check_xml ("test/data/metadata.xml.ref", dir.string() + "/metadata.xml", ignore);
}

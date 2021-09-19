/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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


/** @file  test/film_metadata_test.cc
 *  @brief Test some basic reading/writing of film metadata.
 *  @ingroup feature
 */


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::string;
using std::list;
using std::make_shared;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (film_metadata_test)
{
	auto film = new_test_film ("film_metadata_test");
	auto dir = test_film_dir ("film_metadata_test");

	film->_isdcf_date = boost::gregorian::from_undelimited_string ("20130211");
	BOOST_CHECK (film->container() == Ratio::from_id ("185"));
	BOOST_CHECK (film->dcp_content_type() == nullptr);

	film->set_name ("fred");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("SHR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_j2k_bandwidth (200000000);
	film->set_interop (false);
	film->set_chain (string(""));
	film->set_distributor (string(""));
	film->set_facility (string(""));
	film->set_release_territory (dcp::LanguageTag::RegionSubtag("US"));
	film->write_metadata ();

	list<string> ignore = { "Key", "ContextID" };
	check_xml ("test/data/metadata.xml.ref", dir.string() + "/metadata.xml", ignore);

	auto g = make_shared<Film>(dir);
	g->read_metadata ();

	BOOST_CHECK_EQUAL (g->name(), "fred");
	BOOST_CHECK_EQUAL (g->dcp_content_type(), DCPContentType::from_isdcf_name ("SHR"));
	BOOST_CHECK_EQUAL (g->container(), Ratio::from_id ("185"));

	g->write_metadata ();
	check_xml ("test/data/metadata.xml.ref", dir.string() + "/metadata.xml", ignore);
}


/** Check a bug where <Content> tags with multiple <Text>s would fail to load */
BOOST_AUTO_TEST_CASE (multiple_text_nodes_are_allowed)
{
	auto subs = content_factory("test/data/15s.srt").front();
	auto caps = content_factory("test/data/15s.srt").front();
	auto film = new_test_film2("multiple_text_nodes_are_allowed1", { subs, caps });
	caps->only_text()->set_type(TextType::CLOSED_CAPTION);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE,
			dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME
		});

	auto reload = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2("multiple_text_nodes_are_allowed2", { reload });
	film2->write_metadata ();

	auto test = make_shared<Film>(boost::filesystem::path("build/test/multiple_text_nodes_are_allowed2"));
	test->read_metadata();
}


/** Read some metadata from v2.14.x that fails to open on 2.15.x */
BOOST_AUTO_TEST_CASE (metadata_loads_from_2_14_x_1)
{
	namespace fs = boost::filesystem;
	auto film = make_shared<Film>(fs::path("build/test/metadata_loads_from_2_14_x_1"));
	auto notes = film->read_metadata(fs::path("test/data/2.14.x.metadata.1.xml"));
	BOOST_REQUIRE_EQUAL (notes.size(), 0U);
}


/** Read some more metadata from v2.14.x that fails to open on 2.15.x */
BOOST_AUTO_TEST_CASE (metadata_loads_from_2_14_x_2)
{
	namespace fs = boost::filesystem;
	auto film = make_shared<Film>(fs::path("build/test/metadata_loads_from_2_14_x_2"));
	auto notes = film->read_metadata(fs::path("test/data/2.14.x.metadata.2.xml"));
	BOOST_REQUIRE_EQUAL (notes.size(), 1U);
	BOOST_REQUIRE_EQUAL (notes.front(),
		       "A subtitle or closed caption file in this project is marked with the language 'eng', "
		       "which DCP-o-matic does not recognise.  The file's language has been cleared."
		       );
}


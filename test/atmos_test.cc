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


#include "lib/config.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::string;
using std::vector;
using boost::optional;
using boost::shared_ptr;


BOOST_AUTO_TEST_CASE (atmos_passthrough_test)
{
	shared_ptr<Film> film = new_test_film2 ("atmos_passthrough_test");
	boost::filesystem::path ref = TestPaths::private_data() / "atmos_asset.mxf";
	shared_ptr<Content> content = content_factory (TestPaths::private_data() / "atmos_asset.mxf").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_REQUIRE (mxf_atmos_files_same(ref, dcp_file(film, "atmos"), true));
}


BOOST_AUTO_TEST_CASE (atmos_encrypted_passthrough_test)
{
	shared_ptr<Film> film = new_test_film2 ("atmos_encrypted_passthrough_test");
	boost::filesystem::path ref = TestPaths::private_data() / "atmos_asset.mxf";
	shared_ptr<Content> content = content_factory (TestPaths::private_data() / "atmos_asset.mxf").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_encrypted (true);
	film->_key = dcp::Key ("4fac12927eb122af1c2781aa91f3a4cc");
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_REQUIRE (!mxf_atmos_files_same(ref, dcp_file(film, "atmos")));

	dcp::EncryptedKDM kdm = film->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		vector<string>(),
		dcp_file(film, "cpl"),
		dcp::LocalTime(),
		dcp::LocalTime(),
		dcp::MODIFIED_TRANSITIONAL_1,
		false,
		optional<int>()
		);

	shared_ptr<Film> film2 = new_test_film2 ("atmos_encrypted_passthrough_test2");
	shared_ptr<DCPContent> content2 (new DCPContent(film->dir(film->dcp_name())));
	content2->add_kdm (kdm);
	film2->examine_and_add_content (content2);
	BOOST_REQUIRE (!wait_for_jobs());

	film2->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_CHECK (mxf_atmos_files_same(ref, dcp_file(film2, "atmos"), true));
}


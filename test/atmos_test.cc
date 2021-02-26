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
using std::make_shared;
using boost::optional;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (atmos_passthrough_test)
{
	Cleanup cl;

	auto film = new_test_film2 (
		"atmos_passthrough_test",
		{ content_factory(TestPaths::private_data() / "atmos_asset.mxf").front() },
		&cl
		);

	make_and_verify_dcp (film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});

	auto ref = TestPaths::private_data() / "atmos_asset.mxf";
	BOOST_REQUIRE (mxf_atmos_files_same(ref, dcp_file(film, "atmos"), true));

	cl.run ();
}


BOOST_AUTO_TEST_CASE (atmos_encrypted_passthrough_test)
{
	Cleanup cl;

	auto ref = TestPaths::private_data() / "atmos_asset.mxf";
	auto content = content_factory (TestPaths::private_data() / "atmos_asset.mxf").front();
	auto film = new_test_film2 ("atmos_encrypted_passthrough_test", {content}, &cl);

	film->set_encrypted (true);
	film->_key = dcp::Key ("4fac12927eb122af1c2781aa91f3a4cc");
	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_REQUIRE (!mxf_atmos_files_same(ref, dcp_file(film, "atmos")));

	auto kdm = film->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		vector<string>(),
		dcp_file(film, "cpl"),
		dcp::LocalTime(),
		dcp::LocalTime(),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		false,
		optional<int>()
		);

	auto content2 = make_shared<DCPContent>(film->dir(film->dcp_name()));
	content2->add_kdm (kdm);
	auto film2 = new_test_film2 ("atmos_encrypted_passthrough_test2", {content2}, &cl);
	make_and_verify_dcp (film2, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK (mxf_atmos_files_same(ref, dcp_file(film2, "atmos"), true));

	cl.run ();
}


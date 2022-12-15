/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (dcp_metadata_test)
{
	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2 ("dcp_metadata_test", content);

	Config::instance()->set_dcp_creator ("this is the creator");
	Config::instance()->set_dcp_issuer ("this is the issuer");

	make_and_verify_dcp (
		film,
		{ dcp::VerificationNote::Code::MISSING_CPL_METADATA }
		);

	dcp::DCP dcp (film->dir(film->dcp_name()));
	dcp.read ();
	auto cpls = dcp.cpls();
	BOOST_REQUIRE_EQUAL (cpls.size(), 1U);

	BOOST_CHECK_EQUAL (cpls[0]->creator(), "this is the creator");
	BOOST_CHECK_EQUAL (cpls[0]->issuer(), "this is the issuer");
}


BOOST_AUTO_TEST_CASE(main_picture_active_area_test)
{
	auto content = content_factory(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film2("main_picture_active_area_test", content);
	film->set_resolution(Resolution::FOUR_K);
	film->set_interop(false);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	auto cpls = dcp.cpls();
	BOOST_REQUIRE_EQUAL(cpls.size(), 1U);

	BOOST_REQUIRE(static_cast<bool>(cpls[0]->main_picture_active_area()));
	BOOST_REQUIRE(cpls[0]->main_picture_active_area() == dcp::Size(2866, 2160));
}


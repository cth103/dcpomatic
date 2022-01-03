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
#include "lib/dcp_content.h"
#include "lib/dcp_digest_file.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>


using std::ifstream;
using std::make_shared;
using std::string;
using boost::optional;


BOOST_AUTO_TEST_CASE (dcp_digest_file_test)
{
	dcp::DCP dcp("test/data/dcp_digest_test_dcp");
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);

	write_dcp_digest_file ("build/test/digest.xml", dcp.cpls()[0], "e684e49e89182e907dabe5d9b3bd81ba");

	check_xml ("test/data/digest.xml", "build/test/digest.xml", {});
}


BOOST_AUTO_TEST_CASE (dcp_digest_file_test2)
{
	auto get_key_from_digest = [](boost::filesystem::path filename) -> optional<string> {
		ifstream digest(filename.string().c_str());
		while (digest.good()) {
			string line;
			getline (digest, line);
			boost::algorithm::trim (line);
			if (boost::starts_with(line, "<Key>") && line.length() > 37) {
				return line.substr(5, 32);
			}
		}

		return {};
	};

	auto red = content_factory("test/data/flat_red.png").front();
	auto ov = new_test_film2 ("dcp_digest_file_test2_ov", { red });
	ov->set_encrypted (true);
	make_and_verify_dcp (ov);

	auto ov_key_check = get_key_from_digest ("build/test/dcp_digest_file_test2_ov/" + ov->dcp_name() + ".dcpdig");
	BOOST_REQUIRE (static_cast<bool>(ov_key_check));
	BOOST_CHECK_EQUAL (*ov_key_check, ov->key().hex());

	dcp::DCP find_cpl (ov->dir(ov->dcp_name()));
	find_cpl.read ();
	BOOST_REQUIRE (!find_cpl.cpls().empty());
	auto ov_cpl = find_cpl.cpls()[0]->file();
	BOOST_REQUIRE (static_cast<bool>(ov_cpl));

	auto kdm = ov->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		{},
		ov_cpl.get(),
		dcp::LocalTime(), dcp::LocalTime(),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		true,
		0
		);

	auto ov_dcp = make_shared<DCPContent>(ov->dir(ov->dcp_name()));
	ov_dcp->add_kdm (kdm);
	ov_dcp->set_reference_video (true);
	ov_dcp->set_reference_audio (true);
	auto vf = new_test_film2 ("dcp_digest_file_test2_vf", { ov_dcp });
	vf->set_encrypted (true);
	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	auto vf_key_check = get_key_from_digest ("build/test/dcp_digest_file_test2_vf/" + vf->dcp_name() + ".dcpdig");
	BOOST_REQUIRE (static_cast<bool>(vf_key_check));
	BOOST_CHECK_EQUAL (*vf_key_check, ov->key().hex());
}


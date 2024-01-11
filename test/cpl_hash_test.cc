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


/** @file  test/cpl_hash_test.cc
 *  @brief Make sure that <Hash> tags are always written to CPLs where required.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "test.h"
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE (hash_added_to_imported_dcp_test)
{
	using namespace boost::filesystem;

	string const ov_name = "hash_added_to_imported_dcp_test_ov";
	auto ov = new_test_film2(
		ov_name,
		content_factory("test/data/flat_red.png")
		);
	make_and_verify_dcp (ov);

	/* Remove <Hash> tags from the CPL */
	for (auto i: directory_iterator(String::compose("build/test/%1/%2", ov_name, ov->dcp_name()))) {
		if (boost::algorithm::starts_with(i.path().filename().string(), "cpl_")) {
			dcp::File in(i.path(), "r");
			BOOST_REQUIRE (in);
			dcp::File out(i.path().string() + ".tmp", "w");
			BOOST_REQUIRE (out);
			char buffer[256];
			while (in.gets(buffer, sizeof(buffer))) {
				if (string(buffer).find("Hash") == string::npos) {
					out.puts(buffer);
				}
			}
			in.close();
			out.close();
			rename (i.path().string() + ".tmp", i.path());
		}
	}

	string const vf_name = "hash_added_to_imported_dcp_test_vf";
	auto ov_content = make_shared<DCPContent>(String::compose("build/test/%1/%2", ov_name, ov->dcp_name()));
	auto vf = new_test_film2 (
		vf_name, { ov_content }
		);

	ov_content->set_reference_video (true);
	make_and_verify_dcp(vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET}, false);

	/* Check for Hash tags in the VF DCP */
	int hashes = 0;
	for (auto i: directory_iterator(String::compose("build/test/%1/%2", vf_name, vf->dcp_name()))) {
		if (boost::algorithm::starts_with(i.path().filename().string(), "cpl_")) {
			dcp::File in(i.path(), "r");
			BOOST_REQUIRE (in);
			char buffer[256];
			while (in.gets(buffer, sizeof(buffer))) {
				if (string(buffer).find("Hash") != string::npos) {
					++hashes;
				}
			}
		}
	}
	BOOST_CHECK_EQUAL (hashes, 2);
}


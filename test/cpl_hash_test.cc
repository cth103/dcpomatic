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


using std::string;
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (hash_added_to_imported_dcp_test)
{
	using namespace boost::filesystem;

	string const ov_name = "hash_added_to_imported_dcp_test_ov";
	shared_ptr<Film> ov = new_test_film2 (ov_name);
	ov->examine_and_add_content (content_factory("test/data/flat_red.png").front());
	BOOST_REQUIRE (!wait_for_jobs());
	ov->make_dcp();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Remove <Hash> tags from the CPL */
	for (directory_iterator i = directory_iterator(String::compose("build/test/%1/%2", ov_name, ov->dcp_name())); i != directory_iterator(); ++i) {
		if (boost::algorithm::starts_with(i->path().filename().string(), "cpl_")) {
			FILE* in = fopen_boost(i->path(), "r");
			BOOST_REQUIRE (in);
			FILE* out = fopen_boost(i->path().string() + ".tmp", "w");
			BOOST_REQUIRE (out);
			char buffer[256];
			while (fgets (buffer, sizeof(buffer), in)) {
				if (string(buffer).find("Hash") == string::npos) {
					fputs (buffer, out);
				}
			}
			fclose (in);
			fclose (out);
			rename (i->path().string() + ".tmp", i->path());
		}
	}

	string const vf_name = "hash_added_to_imported_dcp_test_vf";
	shared_ptr<Film> vf = new_test_film2 (vf_name);
	shared_ptr<DCPContent> ov_content(new DCPContent(String::compose("build/test/%1/%2", ov_name, ov->dcp_name())));
	vf->examine_and_add_content (ov_content);
	BOOST_REQUIRE (!wait_for_jobs());

	ov_content->set_reference_video (true);
	vf->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Check for Hash tags in the VF DCP */
	int hashes = 0;
	for (directory_iterator i = directory_iterator(String::compose("build/test/%1/%2", vf_name, vf->dcp_name())); i != directory_iterator(); ++i) {
		if (boost::algorithm::starts_with(i->path().filename().string(), "cpl_")) {
			FILE* in = fopen_boost(i->path(), "r");
			BOOST_REQUIRE (in);
			char buffer[256];
			while (fgets (buffer, sizeof(buffer), in)) {
				if (string(buffer).find("Hash") != string::npos) {
					++hashes;
				}
			}
			fclose (in);
		}
	}
	BOOST_CHECK_EQUAL (hashes, 2);
}


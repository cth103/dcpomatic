/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/digest_test.cc
 *  @brief Check computed DCP digests against references calculated by the `openssl` binary.
 *  @ingroup feature
 */


#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/dcp_content_type.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::list;
using std::string;
using std::shared_ptr;
using std::make_shared;


static string
openssl_hash (boost::filesystem::path file)
{
	auto pipe = popen (String::compose ("openssl sha1 -binary %1 | openssl base64 -e", file.string()).c_str (), "r");
	BOOST_REQUIRE (pipe);
	char buffer[128];
	string output;
	while (!feof (pipe)) {
		if (fgets (buffer, sizeof(buffer), pipe)) {
			output += buffer;
		}
	}
	pclose (pipe);
	if (!output.empty ()) {
		output = output.substr (0, output.length() - 1);
	}
	return output;
}


/** Test the digests made by the DCP writing code on a multi-reel DCP */
BOOST_AUTO_TEST_CASE (digest_test)
{
	auto film = new_test_film ("digest_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_name ("digest_test");
	auto r = make_shared<ImageContent>("test/data/flat_red.png");
	auto g = make_shared<ImageContent>("test/data/flat_green.png");
	auto b = make_shared<ImageContent>("test/data/flat_blue.png");
	film->examine_and_add_content (r);
	film->examine_and_add_content (g);
	film->examine_and_add_content (b);
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE (!wait_for_jobs());

	BOOST_CHECK (Config::instance()->master_encoding_threads() > 1);
	make_and_verify_dcp (film);

	dcp::DCP dcp (film->dir (film->dcp_name ()));
	dcp.read ();
	BOOST_CHECK_EQUAL (dcp.cpls().size(), 1U);
	auto reels = dcp.cpls()[0]->reels();

	auto i = reels.begin ();
	BOOST_REQUIRE (i != reels.end ());
	BOOST_REQUIRE ((*i)->main_picture()->hash());
	BOOST_REQUIRE ((*i)->main_picture()->asset()->file());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file().get()));
	++i;
	BOOST_REQUIRE (i != reels.end ());
	BOOST_REQUIRE ((*i)->main_picture()->hash());
	BOOST_REQUIRE ((*i)->main_picture()->asset()->file());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file().get()));
	++i;
	BOOST_REQUIRE (i != reels.end ());
	BOOST_REQUIRE ((*i)->main_picture()->hash());
	BOOST_REQUIRE ((*i)->main_picture()->asset()->file());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file().get()));
	++i;
	BOOST_REQUIRE (i == reels.end ());
}

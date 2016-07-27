/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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
using boost::shared_ptr;

static string
openssl_hash (boost::filesystem::path file)
{
	FILE* pipe = popen (String::compose ("openssl sha1 -binary %1 | openssl base64 -e", file.string()).c_str (), "r");
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
	shared_ptr<Film> film = new_test_film ("digest_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_name ("digest_test");
	shared_ptr<ImageContent> r (new ImageContent (film, "test/data/flat_red.png"));
	shared_ptr<ImageContent> g (new ImageContent (film, "test/data/flat_green.png"));
	shared_ptr<ImageContent> b (new ImageContent (film, "test/data/flat_blue.png"));
	film->examine_and_add_content (r);
	film->examine_and_add_content (g);
	film->examine_and_add_content (b);
	film->set_reel_type (REELTYPE_BY_VIDEO_CONTENT);
	wait_for_jobs ();

	Config::instance()->set_num_local_encoding_threads (4);
	film->make_dcp ();
	wait_for_jobs ();
	Config::instance()->set_num_local_encoding_threads (1);

	dcp::DCP dcp (film->dir (film->dcp_name ()));
	dcp.read ();
	BOOST_CHECK_EQUAL (dcp.cpls().size(), 1);
	list<shared_ptr<dcp::Reel> > reels = dcp.cpls().front()->reels ();

	list<shared_ptr<dcp::Reel> >::const_iterator i = reels.begin ();
	BOOST_REQUIRE (i != reels.end ());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file ()));
	++i;
	BOOST_REQUIRE (i != reels.end ());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file ()));
	++i;
	BOOST_REQUIRE (i != reels.end ());
	BOOST_CHECK_EQUAL ((*i)->main_picture()->hash().get(), openssl_hash ((*i)->main_picture()->asset()->file ()));
	++i;
	BOOST_REQUIRE (i == reels.end ());
}

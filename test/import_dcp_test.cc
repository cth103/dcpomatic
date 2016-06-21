/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "test.h"
#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/config.h"
#include "lib/cross.h"
#include <dcp/cpl.h>
#include <boost/test/unit_test.hpp>

using std::vector;
using boost::shared_ptr;

/** Make an encrypted DCP, import it and make a new unencrypted DCP */
BOOST_AUTO_TEST_CASE (import_dcp_test)
{
	shared_ptr<Film> A = new_test_film ("import_dcp_test");
	A->set_container (Ratio::from_id ("185"));
	A->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	A->set_name ("frobozz");

	shared_ptr<FFmpegContent> c (new FFmpegContent (A, "test/data/test.mp4"));
	A->examine_and_add_content (c);
	A->set_encrypted (true);
	wait_for_jobs ();

	A->make_dcp ();
	wait_for_jobs ();

	dcp::DCP A_dcp ("build/test/import_dcp_test/" + A->dcp_name());
	A_dcp.read ();

	Config::instance()->set_decryption_chain (shared_ptr<dcp::CertificateChain> (new dcp::CertificateChain (openssl_path ())));

	dcp::EncryptedKDM kdm = A->make_kdm (
		Config::instance()->decryption_chain()->leaf (),
		vector<dcp::Certificate> (),
		A_dcp.cpls().front()->file (),
		dcp::LocalTime ("2014-07-21T00:00:00+00:00"),
		dcp::LocalTime ("2024-07-21T00:00:00+00:00"),
		dcp::MODIFIED_TRANSITIONAL_1
		);

	shared_ptr<Film> B = new_test_film ("import_dcp_test2");
	B->set_container (Ratio::from_id ("185"));
	B->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	B->set_name ("frobozz");

	shared_ptr<DCPContent> d (new DCPContent (B, "build/test/import_dcp_test/" + A->dcp_name()));
	d->add_kdm (kdm);
	B->examine_and_add_content (d);
	wait_for_jobs ();

	B->make_dcp ();
	wait_for_jobs ();

	/* Should be 1s red, 1s green, 1s blue */
	check_dcp ("test/data/import_dcp_test2", "build/test/import_dcp_test2/" + B->dcp_name());
}

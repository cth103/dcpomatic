/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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
BOOST_AUTO_TEST_CASE (vf_kdm_test)
{
	shared_ptr<Film> A = new_test_film ("vf_kdm_test_ov");
	A->set_container (Ratio::from_id ("185"));
	A->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	A->set_name ("frobozz");

	shared_ptr<FFmpegContent> c (new FFmpegContent (A, "test/data/test.mp4"));
	A->examine_and_add_content (c);
	A->set_encrypted (true);
	wait_for_jobs ();
	A->make_dcp ();
	wait_for_jobs ();

	shared_ptr<Film> B = new_test_film ("vf_kdm_test_vf");
	B->set_container (Ratio::from_id ("185"));
	B->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	B->set_name ("frobozz");

	shared_ptr<DCPContent> d (new DCPContent (B, "build/test/vf_kdm_test_ov/" + A->dcp_name()));
	B->examine_and_add_content (d);
	B->set_encrypted (true);
	wait_for_jobs ();
	B->make_dcp ();
	wait_for_jobs ();

	dcp::DCP B_dcp ("build/test/vf_kdm_test_vf/" + B->dcp_name());
	B_dcp.read ();

	Config::instance()->set_decryption_chain (shared_ptr<dcp::CertificateChain> (new dcp::CertificateChain (openssl_path ())));

	dcp::EncryptedKDM kdm = B->make_kdm (
		Config::instance()->decryption_chain()->leaf (),
		vector<dcp::Certificate> (),
		B_dcp.cpls().front()->file().get(),
		dcp::LocalTime ("2014-07-21T00:00:00+00:00"),
		dcp::LocalTime ("2024-07-21T00:00:00+00:00"),
		dcp::MODIFIED_TRANSITIONAL_1
		);

	shared_ptr<Film> C = new_test_film ("vf_kdm_test_check");
	B->set_container (Ratio::from_id ("185"));
	B->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	B->set_name ("frobozz");

	shared_ptr<DCPContent> e (new DCPContent (C, "build/test/vf_kdm_test_vf/" + B->dcp_name()));
	e->add_kdm (kdm);
	C->examine_and_add_content (e);
	wait_for_jobs ();
	C->make_dcp ();
	wait_for_jobs ();

	/* Should be 1s red, 1s green, 1s blue */
	check_dcp ("test/data/vf_kdm_test_check", "build/test/vf_kdm_test_check/" + C->dcp_name());
}

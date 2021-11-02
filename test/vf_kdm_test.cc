/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/vf_kdm_test.cc
 *  @brief Test encrypted VF creation and import
 *  @ingroup feature
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
#include "lib/screen.h"
#include <dcp/cpl.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE (vf_kdm_test)
{
	ConfigRestorer cr;

	/* Make an encrypted DCP from test.mp4 */

	auto A = new_test_film ("vf_kdm_test_ov");
	A->set_container (Ratio::from_id ("185"));
	A->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	A->set_name ("frobozz");
	A->set_interop (true);

	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	A->examine_and_add_content (c);
	A->set_encrypted (true);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (A, {dcp::VerificationNote::Code::INVALID_STANDARD});

	dcp::DCP A_dcp ("build/test/vf_kdm_test_ov/" + A->dcp_name());
	A_dcp.read ();

	Config::instance()->set_decryption_chain (make_shared<dcp::CertificateChain>(openssl_path()));

	auto A_kdm = A->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		vector<string>(),
		A_dcp.cpls().front()->file().get(),
		dcp::LocalTime("2030-07-21T00:00:00+00:00"),
		dcp::LocalTime("2031-07-21T00:00:00+00:00"),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		true, 0
		);

	/* Import A into a new project, with the required KDM, and make a VF that refers to it */

	auto B = new_test_film ("vf_kdm_test_vf");
	B->set_container (Ratio::from_id("185"));
	B->set_dcp_content_type (DCPContentType::from_isdcf_name("TLR"));
	B->set_name ("frobozz");
	B->set_interop (true);

	auto d = make_shared<DCPContent>("build/test/vf_kdm_test_ov/" + A->dcp_name());
	d->add_kdm (A_kdm);
	d->set_reference_video (true);
	B->examine_and_add_content (d);
	B->set_encrypted (true);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (B, {dcp::VerificationNote::Code::INVALID_STANDARD, dcp::VerificationNote::Code::EXTERNAL_ASSET});

	dcp::DCP B_dcp ("build/test/vf_kdm_test_vf/" + B->dcp_name());
	B_dcp.read ();

	auto B_kdm = B->make_kdm (
		Config::instance()->decryption_chain()->leaf (),
		vector<string>(),
		B_dcp.cpls().front()->file().get(),
		dcp::LocalTime ("2030-07-21T00:00:00+00:00"),
		dcp::LocalTime ("2031-07-21T00:00:00+00:00"),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		true, 0
		);

	/* Import the OV and VF into a new project with the KDM that was created for the VF.
	   This KDM should decrypt assets from the OV too.
	*/

	auto C = new_test_film ("vf_kdm_test_check");
	C->set_container (Ratio::from_id ("185"));
	C->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	C->set_name ("frobozz");
	C->set_interop (true);

	auto e = make_shared<DCPContent>("build/test/vf_kdm_test_vf/" + B->dcp_name());
	e->add_kdm (B_kdm);
	e->add_ov ("build/test/vf_kdm_test_ov/" + A->dcp_name());
	C->examine_and_add_content (e);
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (C, {dcp::VerificationNote::Code::INVALID_STANDARD});

	/* Should be 1s red, 1s green, 1s blue */
	check_dcp ("test/data/vf_kdm_test_check", "build/test/vf_kdm_test_check/" + C->dcp_name());
}

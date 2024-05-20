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
#include <dcp/atmos_asset.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


BOOST_AUTO_TEST_CASE (atmos_passthrough_test)
{
	Cleanup cl;

	auto film = new_test_film2 (
		"atmos_passthrough_test",
		content_factory(TestPaths::private_data() / "atmos_asset.mxf"),
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
	auto content = content_factory(TestPaths::private_data() / "atmos_asset.mxf");
	auto film = new_test_film2 ("atmos_encrypted_passthrough_test", content, &cl);

	film->set_encrypted (true);
	film->_key = dcp::Key ("4fac12927eb122af1c2781aa91f3a4cc");
	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_REQUIRE (!mxf_atmos_files_same(ref, dcp_file(film, "atmos")));

	auto signer = Config::instance()->signer_chain();
	BOOST_REQUIRE(signer->valid());

	auto const decrypted_kdm = film->make_kdm(dcp_file(film, "cpl"), dcp::LocalTime(), dcp::LocalTime());
	auto const kdm = decrypted_kdm.encrypt(signer, Config::instance()->decryption_chain()->leaf(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, false, {});

	auto content2 = make_shared<DCPContent>(film->dir(film->dcp_name()));
	content2->add_kdm (kdm);
	auto film2 = new_test_film2 ("atmos_encrypted_passthrough_test2", {content2}, &cl);
	make_and_verify_dcp (film2, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK (mxf_atmos_files_same(ref, dcp_file(film2, "atmos"), true));

	cl.run ();
}


BOOST_AUTO_TEST_CASE (atmos_trim_test)
{
	Cleanup cl;

	auto ref = TestPaths::private_data() / "atmos_asset.mxf";
	auto content = content_factory(TestPaths::private_data() / "atmos_asset.mxf");
	auto film = new_test_film2 ("atmos_trim_test", content, &cl);

	content[0]->set_trim_start(film, dcpomatic::ContentTime::from_seconds(1));

	/* Just check that the encode runs; I'm not sure how to test the MXF */
	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	cl.run();
}


BOOST_AUTO_TEST_CASE(atmos_replace_test)
{
	auto const frames = 240;

	auto check = [](shared_ptr<const Film> film, int data) {
		dcp::DCP dcp(film->dir(film->dcp_name()));
		dcp.read();
		BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
		BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 1U);
		BOOST_REQUIRE(dcp.cpls()[0]->reels()[0]->atmos());
		auto asset = dcp.cpls()[0]->reels()[0]->atmos()->asset();
		BOOST_REQUIRE(asset);
		auto reader = asset->start_read();
		for (int i = 0; i < frames; ++i) {
			auto frame = reader->get_frame(i);
			BOOST_REQUIRE(frame);
			for (int j = 0; j < frame->size(); ++j) {
				BOOST_REQUIRE_EQUAL(frame->data()[j], data);
			}
		}
	};

	auto atmos_0 = content_factory("test/data/atmos_0.mxf");
	auto ov = new_test_film2("atmos_merge_test_ov", atmos_0);
	make_and_verify_dcp(ov, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	// atmos_0.mxf should contain all zeros for its data
	check(ov, 0);

	auto atmos_1 = content_factory("test/data/atmos_1.mxf");
	auto ov_content = std::make_shared<DCPContent>(boost::filesystem::path("build/test/atmos_merge_test_ov") / ov->dcp_name());
	auto vf = new_test_film2("atmos_merge_test_vf", { ov_content, atmos_1.front() });
	ov_content->set_reference_video(true);
	atmos_1.front()->set_position(vf, dcpomatic::DCPTime());
	make_and_verify_dcp(vf, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);
	// atmos_1.mxf should contain all ones for its data, and it should have replaced atmos_0 in this DCP
	check(vf, 1);
}


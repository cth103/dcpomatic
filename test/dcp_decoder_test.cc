/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/dcp_decoder_test.cc
 *  @brief Test DCPDecoder class.
 *  @ingroup selfcontained
 */


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_decoder.h"
#include "lib/examine_content_job.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/piece.h"
#include "lib/player.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


/* Check that DCPDecoder reuses old data when it should */
BOOST_AUTO_TEST_CASE (check_reuse_old_data_test)
{
	/* Make some DCPs */

	auto ov = new_test_film2 ("check_reuse_old_data_ov", {content_factory("test/data/flat_red.png").front()});
	make_and_verify_dcp (ov);

	auto ov_content = make_shared<DCPContent>(ov->dir(ov->dcp_name(false)));
	auto vf = new_test_film2 ("check_reuse_old_data_vf", {ov_content, content_factory("test/data/L.wav").front()});
	ov_content->set_reference_video (true);
	make_and_verify_dcp (vf, {dcp::VerificationNote::Code::EXTERNAL_ASSET});

	auto encrypted = new_test_film2 ("check_reuse_old_data_decrypted");
	encrypted->examine_and_add_content (content_factory("test/data/flat_red.png").front());
	BOOST_REQUIRE (!wait_for_jobs());
	encrypted->set_encrypted (true);
	make_and_verify_dcp (encrypted);

	dcp::DCP encrypted_dcp (encrypted->dir(encrypted->dcp_name()));
	encrypted_dcp.read ();

	auto kdm = encrypted->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		vector<string>(),
		encrypted_dcp.cpls().front()->file().get(),
		dcp::LocalTime ("2030-07-21T00:00:00+00:00"),
		dcp::LocalTime ("2031-07-21T00:00:00+00:00"),
		dcp::Formulation::MODIFIED_TRANSITIONAL_1,
		true, 0
		);


	/* Add just the OV to a new project, move it around a bit and check that
	   the _reels get reused.
	*/
	auto test = new_test_film2 ("check_reuse_old_data_test1");
	ov_content = make_shared<DCPContent>(ov->dir(ov->dcp_name(false)));
	test->examine_and_add_content (ov_content);
	BOOST_REQUIRE (!wait_for_jobs());
	auto player = make_shared<Player>(test, Image::Alignment::COMPACT);

	auto decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	auto reels = decoder->reels();

	ov_content->set_position (test, dcpomatic::DCPTime(96000));
	decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	BOOST_REQUIRE (reels == decoder->reels());

	/* Add the VF to a new project, then add the OV and check that the
	   _reels did not get reused.
	*/
	test = new_test_film2 ("check_reuse_old_data_test2");
	auto vf_content = make_shared<DCPContent>(vf->dir(vf->dcp_name(false)));
	test->examine_and_add_content (vf_content);
	BOOST_REQUIRE (!wait_for_jobs());
	player = make_shared<Player>(test, Image::Alignment::COMPACT);

	decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	reels = decoder->reels();

	vf_content->add_ov (ov->dir(ov->dcp_name(false)));
	JobManager::instance()->add (make_shared<ExamineContentJob>(test, vf_content));
	BOOST_REQUIRE (!wait_for_jobs());
	decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	BOOST_REQUIRE (reels != decoder->reels());

	/* Add a KDM to an encrypted DCP and check that the _reels did not get reused */
	test = new_test_film2 ("check_reuse_old_data_test3");
	auto encrypted_content = make_shared<DCPContent>(encrypted->dir(encrypted->dcp_name(false)));
	test->examine_and_add_content (encrypted_content);
	BOOST_REQUIRE (!wait_for_jobs());
	player = make_shared<Player>(test, Image::Alignment::COMPACT);

	decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	reels = decoder->reels();

	encrypted_content->add_kdm (kdm);
	JobManager::instance()->add (make_shared<ExamineContentJob>(test, encrypted_content));
	BOOST_REQUIRE (!wait_for_jobs());
	decoder = std::dynamic_pointer_cast<DCPDecoder>(player->_pieces.front()->decoder);
	BOOST_REQUIRE (decoder);
	BOOST_REQUIRE (reels != decoder->reels());
}

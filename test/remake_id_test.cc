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


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/examine_content_job.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::string;
using std::vector;
using boost::optional;


/** Check for bug #1126 whereby making a new DCP using the same video asset as an old one
 *  corrupts the old one.
 */
BOOST_AUTO_TEST_CASE (remake_id_test1)
{
	/* Make a DCP */
	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film("remake_id_test1_1", content);
	make_and_verify_dcp (film);

	/* Copy the video file */
	auto first_video = dcp_file(film, "j2c");
	boost::filesystem::copy_file (first_video, first_video.string() + ".copy");

	/* Make a new DCP with the same video file */
	film->set_name ("remake_id_test1_2");
	make_and_verify_dcp (film);

	/* Check that the video in the first DCP hasn't changed */
	check_file (first_video, first_video.string() + ".copy");
}


/** Check for bug #1232 where remaking an encrypted DCP causes problems with HMAC IDs (?) */
BOOST_AUTO_TEST_CASE (remake_id_test2)
{
	/* Make a DCP */
	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film("remake_id_test2_1", content);
	film->set_encrypted (true);
	make_and_verify_dcp (film);

	/* Remove and remake it */
	boost::filesystem::remove_all(film->dir(film->dcp_name()));
	make_and_verify_dcp (film);

	/* Find the CPL */
	optional<boost::filesystem::path> cpl;
	for (auto i: boost::filesystem::directory_iterator(film->dir(film->dcp_name()))) {
		if (i.path().filename().string().substr(0, 4) == "cpl_") {
			cpl = i.path();
		}
	}
	BOOST_REQUIRE(cpl);

	auto signer = Config::instance()->signer_chain();
	BOOST_REQUIRE(signer->valid());

	/* Make a DKDM */
	auto const decrypted_kdm = film->make_kdm(*cpl, dcp::LocalTime ("2030-01-01T01:00:00+00:00"), dcp::LocalTime ("2031-01-01T01:00:00+00:00"));
	auto const kdm = decrypted_kdm.encrypt(signer, Config::instance()->decryption_chain()->leaf(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, true, 0);

	/* Import the DCP into a new film */
	auto dcp_content = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film("remake_id_test2_2", { dcp_content });
	dcp_content->add_kdm(kdm);
	JobManager::instance()->add(make_shared<ExamineContentJob>(film2, dcp_content));
	BOOST_REQUIRE(!wait_for_jobs());
	make_and_verify_dcp (film2);
}

/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/subtitle_content.h"
#include "lib/job_manager.h"
#include "lib/film.h"
#include "lib/dcp_content.h"
#include "lib/examine_content_job.h"
#include "lib/config.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::string;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

/** Check for bug #1126 whereby making a new DCP using the same video asset as an old one
 *  corrupts the old one.
 */
BOOST_AUTO_TEST_CASE (remake_id_test1)
{
	/* Make a DCP */
	shared_ptr<Film> film = new_test_film2 ("remake_id_test1_1");
	shared_ptr<Content> content = content_factory(film, "test/data/flat_red.png").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Copy the video file */
	boost::filesystem::path first_video = dcp_file(film, "j2c");
	boost::filesystem::copy_file (first_video, first_video.string() + ".copy");

	/* Make a new DCP with the same video file */
	film->set_name ("remake_id_test1_2");
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Check that the video in the first DCP hasn't changed */
	check_file (first_video, first_video.string() + ".copy");
}

/** Check for bug #1232 where remaking an encrypted DCP causes problems with HMAC IDs (?) */
BOOST_AUTO_TEST_CASE (remake_id_test2)
{
	/* Make a DCP */
	shared_ptr<Film> film = new_test_film2 ("remake_id_test2_1");
	shared_ptr<Content> content = content_factory(film, "test/data/flat_red.png").front();
	film->examine_and_add_content (content);
	film->set_encrypted (true);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Remake it */
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Find the CPL */
	optional<boost::filesystem::path> cpl;
	for (boost::filesystem::directory_iterator i(film->dir(film->dcp_name())); i != boost::filesystem::directory_iterator(); ++i) {
		if (i->path().filename().string().substr(0, 4) == "cpl_") {
			cpl = i->path();
		}
	}
	BOOST_REQUIRE(cpl);

	/* Make a DKDM */
	dcp::EncryptedKDM kdm = film->make_kdm (
		Config::instance()->decryption_chain()->leaf(),
		vector<dcp::Certificate>(),
		*cpl,
		dcp::LocalTime ("2012-01-01T01:00:00+00:00"),
		dcp::LocalTime ("2112-01-01T01:00:00+00:00"),
		dcp::MODIFIED_TRANSITIONAL_1,
		true,
		0
		);

	/* Import the DCP into a new film */
	shared_ptr<Film> film2 = new_test_film2("remake_id_test2_2");
	shared_ptr<DCPContent> dcp_content(new DCPContent(film2, film->dir(film->dcp_name())));
	film->examine_and_add_content(dcp_content);
	BOOST_REQUIRE(!wait_for_jobs());
	dcp_content->add_kdm(kdm);
	JobManager::instance()->add(shared_ptr<Job>(new ExamineContentJob(film2, dcp_content)));
	BOOST_REQUIRE(!wait_for_jobs());
	film->make_dcp();
	BOOST_REQUIRE(!wait_for_jobs());
}

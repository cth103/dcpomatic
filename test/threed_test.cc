/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

/** @file  test/threed_test.cc
 *  @brief Create some 3D DCPs (without comparing the results to anything).
 *  @ingroup completedcp
 */

#include "test.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/config.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/video_content.h"
#include "lib/job_manager.h"
#include "lib/cross.h"
#include "lib/job.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::shared_ptr;


/** Basic sanity check of THREE_D_LEFT_RIGHT */
BOOST_AUTO_TEST_CASE (threed_test1)
{
	auto film = new_test_film ("threed_test1");
	film->set_name ("test_film1");
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D_LEFT_RIGHT);

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Basic sanity check of THREE_D_ALTERNATE; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test2)
{
	auto film = new_test_film ("threed_test2");
	film->set_name ("test_film2");
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D_ALTERNATE);

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Basic sanity check of THREE_D_LEFT and THREE_D_RIGHT; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test3)
{
	shared_ptr<Film> film = new_test_film2 ("threed_test3");
	auto L = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

BOOST_AUTO_TEST_CASE (threed_test4)
{
	auto film = new_test_film2 ("threed_test4");
	auto L = make_shared<FFmpegContent>(TestPaths::private_data() / "LEFT_TEST_DCP3D4K.mov");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>(TestPaths::private_data() / "RIGHT_TEST_DCP3D4K.mov");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	/* There doesn't seem much point in encoding the whole input, especially as we're only
	 * checking for errors during the encode and not the result.  Also decoding these files
	 * (4K HQ Prores) is very slow.
	 */
	L->set_trim_end (dcpomatic::ContentTime::from_seconds(22));
	R->set_trim_end (dcpomatic::ContentTime::from_seconds(22));

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

BOOST_AUTO_TEST_CASE (threed_test5)
{
	auto film = new_test_film2 ("threed_test5");
	auto L = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	/* There doesn't seem much point in encoding the whole input, especially as we're only
	 * checking for errors during the encode and not the result.
	 */
	L->set_trim_end (dcpomatic::ContentTime::from_seconds(3 * 60 + 20));
	R->set_trim_end (dcpomatic::ContentTime::from_seconds(3 * 60 + 20));

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs ());
}

BOOST_AUTO_TEST_CASE (threed_test6)
{
	auto film = new_test_film2 ("threed_test6");
	auto L = make_shared<FFmpegContent>("test/data/3dL.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/3dR.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	BOOST_REQUIRE (!wait_for_jobs());
	check_dcp ("test/data/threed_test6", film->dir(film->dcp_name()));
}

/** Check 2D content set as being 3D; this should give an informative error */
BOOST_AUTO_TEST_CASE (threed_test7)
{
	using boost::filesystem::path;

	auto film = new_test_film2 ("threed_test7");
	path const content_path = "test/data/red_24.mp4";
	auto c = make_shared<FFmpegContent>(content_path);
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D);

	film->set_three_d (true);
	film->make_dcp ();
	film->write_metadata ();

	auto jm = JobManager::instance ();
	while (jm->work_to_do ()) {
		while (signal_manager->ui_idle()) {}
		dcpomatic_sleep_seconds (1);
	}

	while (signal_manager->ui_idle ()) {}

	BOOST_REQUIRE (jm->errors());
	shared_ptr<Job> failed;
	for (auto i: jm->_jobs) {
		if (i->finished_in_error()) {
			BOOST_REQUIRE (!failed);
			failed = i;
		}
	}
	BOOST_REQUIRE (failed);
	BOOST_CHECK_EQUAL (failed->error_summary(), String::compose("The content file %1 is set as 3D but does not appear to contain 3D images.  Please set it to 2D.  You can still make a 3D DCP from this content by ticking the 3D option in the DCP video tab.", content_path.string()));

	while (signal_manager->ui_idle ()) {}

	JobManager::drop ();
}

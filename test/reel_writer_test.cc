/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

/** @file  test/reel_writer_test.cc
 *  @brief Test ReelWriter class.
 *  @ingroup selfcontained
 */

#include "lib/reel_writer.h"
#include "lib/film.h"
#include "lib/cross.h"
#include "lib/content_factory.h"
#include "lib/content.h"
#include "lib/audio_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>

using std::string;
using std::shared_ptr;
using boost::optional;

static bool equal (dcp::FrameInfo a, ReelWriter const & writer, shared_ptr<InfoFileHandle> file, Frame frame, Eyes eyes)
{
	dcp::FrameInfo b = writer.read_frame_info(file, frame, eyes);
	return a.offset == b.offset && a.size == b.size && a.hash == b.hash;
}

BOOST_AUTO_TEST_CASE (write_frame_info_test)
{
	shared_ptr<Film> film = new_test_film2 ("write_frame_info_test");
	dcpomatic::DCPTimePeriod const period (dcpomatic::DCPTime(0), dcpomatic::DCPTime(96000));
	ReelWriter writer (film, period, shared_ptr<Job>(), 0, 1, false);

	/* Write the first one */

	dcp::FrameInfo info1(0, 123, "12345678901234567890123456789012");
	writer.write_frame_info (0, EYES_LEFT, info1);
	{
		shared_ptr<InfoFileHandle> file = film->info_file_handle(period, true);
		BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
	}

	/* Write some more */

	dcp::FrameInfo info2(596, 14921, "123acb789f1234ae782012n456339522");
	writer.write_frame_info (5, EYES_RIGHT, info2);

	{
		shared_ptr<InfoFileHandle> file = film->info_file_handle(period, true);
		BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
		BOOST_CHECK (equal(info2, writer, file, 5, EYES_RIGHT));
	}

	dcp::FrameInfo info3(12494, 99157123, "xxxxyyyyabc12356ffsfdsf456339522");
	writer.write_frame_info (10, EYES_LEFT, info3);

	{
		shared_ptr<InfoFileHandle> file = film->info_file_handle(period, true);
		BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
		BOOST_CHECK (equal(info2, writer, file, 5, EYES_RIGHT));
		BOOST_CHECK (equal(info3, writer, file, 10, EYES_LEFT));
	}

	/* Overwrite one */

	dcp::FrameInfo info4(55512494, 123599157123, "ABCDEFGyabc12356ffsfdsf4563395ZZ");
	writer.write_frame_info (5, EYES_RIGHT, info4);

	{
		shared_ptr<InfoFileHandle> file = film->info_file_handle(period, true);
		BOOST_CHECK (equal(info1, writer, file, 0, EYES_LEFT));
		BOOST_CHECK (equal(info4, writer, file, 5, EYES_RIGHT));
		BOOST_CHECK (equal(info3, writer, file, 10, EYES_LEFT));
	}
}

/** Check that the reel writer correctly re-uses a video asset changed if we remake
 *  a DCP with no video changes.
 */
BOOST_AUTO_TEST_CASE (reel_reuse_video_test)
{
	/* Make a DCP */
	shared_ptr<Film> film = new_test_film2 ("reel_reuse_video_test");
	shared_ptr<Content> video = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());
	shared_ptr<Content> audio = content_factory("test/data/white.wav").front();
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Find main picture and sound asset IDs */
	dcp::DCP dcp1 (film->dir(film->dcp_name()));
	dcp1.read ();
	BOOST_REQUIRE_EQUAL (dcp1.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp1.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (dcp1.cpls().front()->reels().front()->main_picture());
	BOOST_REQUIRE (dcp1.cpls().front()->reels().front()->main_sound());
	string const picture_id = dcp1.cpls().front()->reels().front()->main_picture()->asset()->id();
	string const sound_id = dcp1.cpls().front()->reels().front()->main_sound()->asset()->id();

	/* Change the audio and re-make */
	audio->audio->set_gain (-3);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Video ID should be the same, sound different */
	dcp::DCP dcp2 (film->dir(film->dcp_name()));
	dcp2.read ();
	BOOST_REQUIRE_EQUAL (dcp2.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp2.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (dcp2.cpls().front()->reels().front()->main_picture());
	BOOST_REQUIRE (dcp2.cpls().front()->reels().front()->main_sound());
	BOOST_CHECK_EQUAL (picture_id, dcp2.cpls().front()->reels().front()->main_picture()->asset()->id());
	BOOST_CHECK (sound_id != dcp2.cpls().front()->reels().front()->main_sound()->asset()->id());

	/* Crop video and re-make */
	video->video->set_left_crop (5);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Video and sound IDs should be different */
	dcp::DCP dcp3 (film->dir(film->dcp_name()));
	dcp3.read ();
	BOOST_REQUIRE_EQUAL (dcp3.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp3.cpls().front()->reels().size(), 1U);
	BOOST_REQUIRE (dcp3.cpls().front()->reels().front()->main_picture());
	BOOST_REQUIRE (dcp3.cpls().front()->reels().front()->main_sound());
	BOOST_CHECK (picture_id != dcp3.cpls().front()->reels().front()->main_picture()->asset()->id());
	BOOST_CHECK (sound_id != dcp3.cpls().front()->reels().front()->main_sound()->asset()->id());
}

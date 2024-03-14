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


/** @file  test/reel_writer_test.cc
 *  @brief Test ReelWriter class.
 *  @ingroup selfcontained
 */


#include "lib/audio_content.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/film.h"
#include "lib/frame_info.h"
#include "lib/reel_writer.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::string;
using boost::optional;


static bool equal(J2KFrameInfo a, shared_ptr<InfoFileHandle> file, Frame frame, Eyes eyes)
{
	auto b = J2KFrameInfo(file, frame, eyes);
	return a.offset == b.offset && a.size == b.size && a.hash == b.hash;
}


BOOST_AUTO_TEST_CASE (write_frame_info_test)
{
	auto film = new_test_film2 ("write_frame_info_test");
	dcpomatic::DCPTimePeriod const period (dcpomatic::DCPTime(0), dcpomatic::DCPTime(96000));
	ReelWriter writer(film, period, shared_ptr<Job>(), 0, 1, false, "foo");

	/* Write the first one */

	J2KFrameInfo info1(0, 123, "12345678901234567890123456789012");
	info1.write(film->info_file_handle(period, false), 0, Eyes::LEFT);
	{
		auto file = film->info_file_handle(period, true);
		BOOST_CHECK(equal(info1, file, 0, Eyes::LEFT));
	}

	/* Write some more */

	J2KFrameInfo info2(596, 14921, "123acb789f1234ae782012n456339522");
	info2.write(film->info_file_handle(period, false), 5, Eyes::RIGHT);

	{
		auto file = film->info_file_handle(period, true);
		BOOST_CHECK(equal(info1, file, 0, Eyes::LEFT));
		BOOST_CHECK(equal(info2, file, 5, Eyes::RIGHT));
	}

	J2KFrameInfo info3(12494, 99157123, "xxxxyyyyabc12356ffsfdsf456339522");
	info3.write(film->info_file_handle(period, false), 10, Eyes::LEFT);

	{
		auto file = film->info_file_handle(period, true);
		BOOST_CHECK(equal(info1, file, 0, Eyes::LEFT));
		BOOST_CHECK(equal(info2, file, 5, Eyes::RIGHT));
		BOOST_CHECK(equal(info3, file, 10, Eyes::LEFT));
	}

	/* Overwrite one */

	J2KFrameInfo info4(55512494, 123599157123, "ABCDEFGyabc12356ffsfdsf4563395ZZ");
	info4.write(film->info_file_handle(period, false), 5, Eyes::RIGHT);

	{
		auto file = film->info_file_handle(period, true);
		BOOST_CHECK(equal(info1, file, 0, Eyes::LEFT));
		BOOST_CHECK(equal(info4, file, 5, Eyes::RIGHT));
		BOOST_CHECK(equal(info3, file, 10, Eyes::LEFT));
	}
}


/** Check that the reel writer correctly re-uses a video asset changed if we remake
 *  a DCP with no video changes.
 */
BOOST_AUTO_TEST_CASE (reel_reuse_video_test)
{
	/* Make a DCP */
	auto video = content_factory("test/data/flat_red.png")[0];
	auto audio = content_factory("test/data/white.wav")[0];
	auto film = new_test_film2 ("reel_reuse_video_test", { video, audio });
	make_and_verify_dcp (film);

	/* Find main picture and sound asset IDs */
	dcp::DCP dcp1 (film->dir(film->dcp_name()));
	dcp1.read ();
	BOOST_REQUIRE_EQUAL (dcp1.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp1.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (dcp1.cpls()[0]->reels()[0]->main_picture());
	BOOST_REQUIRE (dcp1.cpls()[0]->reels()[0]->main_sound());
	auto const picture_id = dcp1.cpls()[0]->reels()[0]->main_picture()->asset()->id();
	auto const sound_id = dcp1.cpls()[0]->reels()[0]->main_sound()->asset()->id();

	/* Change the audio and re-make */
	audio->audio->set_gain (-3);
	/* >1 CPLs in the DCP raises an error in ClairMeta */
	make_and_verify_dcp(film, {}, true, false);

	/* Video ID should be the same, sound different */
	dcp::DCP dcp2 (film->dir(film->dcp_name()));
	dcp2.read ();
	BOOST_REQUIRE_EQUAL (dcp2.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp2.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (dcp2.cpls()[0]->reels()[0]->main_picture());
	BOOST_REQUIRE (dcp2.cpls()[0]->reels()[0]->main_sound());
	BOOST_CHECK_EQUAL (picture_id, dcp2.cpls()[0]->reels()[0]->main_picture()->asset()->id());
	BOOST_CHECK (sound_id != dcp2.cpls()[0]->reels().front()->main_sound()->asset()->id());

	/* Crop video and re-make */
	video->video->set_left_crop (5);
	/* >1 CPLs in the DCP raises an error in ClairMeta */
	make_and_verify_dcp(film, {}, true, false);

	/* Video and sound IDs should be different */
	dcp::DCP dcp3 (film->dir(film->dcp_name()));
	dcp3.read ();
	BOOST_REQUIRE_EQUAL (dcp3.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL (dcp3.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE (dcp3.cpls()[0]->reels()[0]->main_picture());
	BOOST_REQUIRE (dcp3.cpls()[0]->reels()[0]->main_sound());
	BOOST_CHECK (picture_id != dcp3.cpls()[0]->reels()[0]->main_picture()->asset()->id());
	BOOST_CHECK (sound_id != dcp3.cpls()[0]->reels().front()->main_sound()->asset()->id());
}

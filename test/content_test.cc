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

/** @file  test/content_test.cc
 *  @brief Tests which expose problems with certain pieces of content.
 *  @ingroup completedcp
 */

#include "lib/audio_content.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/content_factory.h"
#include "lib/content.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;
using namespace dcpomatic;

/** There has been garbled audio with this piece of content */
BOOST_AUTO_TEST_CASE (content_test1)
{
	shared_ptr<Film> film = new_test_film ("content_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("content_test1");
	film->set_container (Ratio::from_id ("185"));

	shared_ptr<Content> content = content_factory(TestPaths::private_data() / "demo_sound_bug.mkv").front ();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());

	boost::filesystem::path check;

	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator("build/test/content_test1/" + film->dcp_name());
		i != boost::filesystem::directory_iterator();
		++i) {

		if (i->path().leaf().string().substr(0, 4) == "pcm_") {
			check = *i;
		}
	}

	check_mxf_audio_file (TestPaths::private_data() / "content_test1.mxf", check);
}

/** Taking some 23.976fps content and trimming 0.5s (in content time) from the start
 *  has failed in the past; ensure that this is fixed.
 */
BOOST_AUTO_TEST_CASE (content_test2)
{
	shared_ptr<Film> film = new_test_film2 ("content_test2");

	shared_ptr<Content> content = content_factory("test/data/red_23976.mp4").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());
	content->set_trim_start(ContentTime::from_seconds(0.5));
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs ());
}

/** Check that position and start trim of video content is forced to a frame boundary */
BOOST_AUTO_TEST_CASE (content_test3)
{
	shared_ptr<Film> film = new_test_film2 ("content_test3");
	film->set_sequence (false);

	shared_ptr<Content> content = content_factory("test/data/red_24.mp4").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	/* Trim */

	/* 12 frames */
	content->set_trim_start (ContentTime::from_seconds (12.0 / 24.0));
	BOOST_CHECK (content->trim_start() == ContentTime::from_seconds (12.0 / 24.0));

	/* 11.2 frames */
	content->set_trim_start (ContentTime::from_seconds (11.2 / 24.0));
	BOOST_CHECK (content->trim_start() == ContentTime::from_seconds (11.0 / 24.0));

	/* 13.9 frames */
	content->set_trim_start (ContentTime::from_seconds (13.9 / 24.0));
	BOOST_CHECK (content->trim_start() == ContentTime::from_seconds (14.0 / 24.0));

	/* Position */

	/* 12 frames */
	content->set_position (film, DCPTime::from_seconds(12.0 / 24.0));
	BOOST_CHECK (content->position() == DCPTime::from_seconds (12.0 / 24.0));

	/* 11.2 frames */
	content->set_position (film, DCPTime::from_seconds(11.2 / 24.0));
	BOOST_CHECK (content->position() == DCPTime::from_seconds (11.0 / 24.0));

	/* 13.9 frames */
	content->set_position (film, DCPTime::from_seconds(13.9 / 24.0));
	BOOST_CHECK (content->position() == DCPTime::from_seconds (14.0 / 24.0));

	content->set_video_frame_rate (25);

	/* Check that trim is fixed when the content's video frame rate is `forced' */

	BOOST_CHECK (content->trim_start() == ContentTime::from_seconds (15.0 / 25.0));
}


/** Content containing video will have its length rounded to the nearest video frame */
BOOST_AUTO_TEST_CASE (content_test4)
{
	shared_ptr<Film> film = new_test_film2 ("content_test4");

	shared_ptr<Content> video = content_factory("test/data/count300bd24.m2ts").front();
	film->examine_and_add_content (video);
	BOOST_REQUIRE (!wait_for_jobs());

	video->set_trim_end (dcpomatic::ContentTime(3000));
	BOOST_CHECK (video->length_after_trim(film) == DCPTime::from_frames(299, 24));
}


/** Content containing no video will not have its length rounded to the nearest video frame */
BOOST_AUTO_TEST_CASE (content_test5)
{
	shared_ptr<Film> film = new_test_film2 ("content_test5");

	shared_ptr<Content> audio = content_factory("test/data/sine_16_48_220_10.wav").front();
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs());

	audio->set_trim_end (dcpomatic::ContentTime(3000));
	BOOST_CHECK (audio->length_after_trim(film) == DCPTime(957000));
}


/** Sync error #1833 */
BOOST_AUTO_TEST_CASE (content_test6)
{
	shared_ptr<Film> film = new_test_film2 ("content_test6");
	film->examine_and_add_content (content_factory(TestPaths::private_data() / "fha.mkv").front());
	BOOST_REQUIRE (!wait_for_jobs());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
	check_dcp (TestPaths::private_data() / "fha", film);
}


/** Reel length error when making the test for #1833 */
BOOST_AUTO_TEST_CASE (content_test7)
{
	shared_ptr<Film> film = new_test_film2 ("content_test7");
	shared_ptr<Content> content = content_factory(TestPaths::private_data() / "clapperboard.mp4").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	content->audio->set_delay (-1000);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

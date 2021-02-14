/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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

/** @file  test/empty_test.cc
 *  @brief Test the creation of Empty objects.
 *  @ingroup feature
 */

#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "lib/image_content.h"
#include "lib/empty.h"
#include "lib/player.h"
#include "lib/decoder.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::list;
using std::make_shared;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;

bool
has_video (shared_ptr<const Content> content)
{
        return static_cast<bool>(content->video);
}

BOOST_AUTO_TEST_CASE (empty_test1)
{
	shared_ptr<Film> film = new_test_film2 ("empty_test1");
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent("test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent("test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 *     A A A     B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime::from_frames (2, vfr));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames (7, vfr));

	Empty black (film, film->playlist(), bind(&has_video, _1), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 2U);
	list<dcpomatic::DCPTimePeriod>::const_iterator i = black._periods.begin();
	BOOST_CHECK (i->from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (i->to ==   DCPTime::from_frames(2, vfr));
	++i;
	BOOST_CHECK (i->from == DCPTime::from_frames(5, vfr));
	BOOST_CHECK (i->to ==   DCPTime::from_frames(7, vfr));
}

/** Some tests where the first empty period is not at time 0 */
BOOST_AUTO_TEST_CASE (empty_test2)
{
	shared_ptr<Film> film = new_test_film2 ("empty_test2");
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent("test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent("test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	Empty black (film, film->playlist(), bind(&has_video, _1), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 1U);
	BOOST_CHECK (black._periods.front().from == DCPTime::from_frames(3, vfr));
	BOOST_CHECK (black._periods.front().to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (black.position() == DCPTime::from_frames(3, vfr));

	/* check that done() works */
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (4, vfr));
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (7, vfr));
	BOOST_CHECK (black.done ());
}

/** Test for when the film's playlist is not the same as the one passed into Empty */
BOOST_AUTO_TEST_CASE (empty_test3)
{
	shared_ptr<Film> film = new_test_film2 ("empty_test3");
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent("test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent("test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	shared_ptr<Playlist> playlist (new Playlist);
	playlist->add (film, contentB);
	Empty black (film, playlist, bind(&has_video, _1), playlist->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 1U);
	BOOST_CHECK (black._periods.front().from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (black._periods.front().to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (black.position() == DCPTime::from_frames(0, vfr));
}

BOOST_AUTO_TEST_CASE (empty_test_with_overlapping_content)
{
	auto film = new_test_film2 ("empty_test_with_overlapping_content");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	contentA->video->set_length (vfr * 3);
	contentA->set_position (film, DCPTime());
	contentB->video->set_length (vfr * 1);
	contentB->set_position (film, DCPTime::from_seconds(1));

	Empty black(film, film->playlist(), bind(&has_video, _1), film->playlist()->length(film));

	BOOST_REQUIRE (black._periods.empty());
}


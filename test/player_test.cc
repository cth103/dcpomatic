/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  test/player_test.cc
 *  @brief Various tests of Player.
 */

#include <iostream>
#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/audio_buffers.h"
#include "lib/player.h"
#include "test.h"

using std::cout;
using std::list;
using boost::shared_ptr;

/** Player::overlaps */
BOOST_AUTO_TEST_CASE (player_overlaps_test)
{
	shared_ptr<Film> film = new_test_film ("player_overlaps_test");
	film->set_container (Ratio::from_id ("185"));
	shared_ptr<FFmpegContent> A (new FFmpegContent (film, "test/data/test.mp4"));
	shared_ptr<FFmpegContent> B (new FFmpegContent (film, "test/data/test.mp4"));
	shared_ptr<FFmpegContent> C (new FFmpegContent (film, "test/data/test.mp4"));

	film->examine_and_add_content (A, true);
	film->examine_and_add_content (B, true);
	film->examine_and_add_content (C, true);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (A->full_length(), DCPTime (288000));

	A->set_position (DCPTime::from_seconds (0));
	B->set_position (DCPTime::from_seconds (10));
	C->set_position (DCPTime::from_seconds (20));

	shared_ptr<Player> player = film->make_player ();

	list<shared_ptr<Piece> > o = player->overlaps<FFmpegContent> (DCPTime::from_seconds (0), DCPTime::from_seconds (5));
	BOOST_CHECK_EQUAL (o.size(), 1);
	BOOST_CHECK_EQUAL (o.front()->content, A);

	o = player->overlaps<FFmpegContent> (DCPTime::from_seconds (5), DCPTime::from_seconds (8));
	BOOST_CHECK_EQUAL (o.size(), 0);

	o = player->overlaps<FFmpegContent> (DCPTime::from_seconds (8), DCPTime::from_seconds (12));
	BOOST_CHECK_EQUAL (o.size(), 1);
	BOOST_CHECK_EQUAL (o.front()->content, B);

	o = player->overlaps<FFmpegContent> (DCPTime::from_seconds (2), DCPTime::from_seconds (12));
	BOOST_CHECK_EQUAL (o.size(), 2);
	BOOST_CHECK_EQUAL (o.front()->content, A);
	BOOST_CHECK_EQUAL (o.back()->content, B);

	o = player->overlaps<FFmpegContent> (DCPTime::from_seconds (8), DCPTime::from_seconds (11));
	BOOST_CHECK_EQUAL (o.size(), 1);
	BOOST_CHECK_EQUAL (o.front()->content, B);
}

/** Check that the Player correctly generates silence when used with a silent FFmpegContent */
BOOST_AUTO_TEST_CASE (player_silence_padding_test)
{
	shared_ptr<Film> film = new_test_film ("player_silence_padding_test");
	film->set_name ("player_silence_padding_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);
	
	film->examine_and_add_content (c, true);
	wait_for_jobs ();

	shared_ptr<Player> player = film->make_player ();
	shared_ptr<AudioBuffers> test = player->get_audio (DCPTime (0), DCPTime::from_seconds (1), true);
	BOOST_CHECK_EQUAL (test->frames(), 48000);
	BOOST_CHECK_EQUAL (test->channels(), film->audio_channels ());

	for (int i = 0; i < test->frames(); ++i) {
		for (int c = 0; c < test->channels(); ++c) {
			BOOST_CHECK_EQUAL (test->data()[c][i], 0);
		}
	}
}


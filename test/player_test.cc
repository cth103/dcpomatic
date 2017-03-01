/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/player_test.cc
 *  @brief Test Player class.
 *  @ingroup selfcontained
 */

#include <iostream>
#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/audio_buffers.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "lib/image_content.h"
#include "test.h"

using std::cout;
using std::list;
using boost::shared_ptr;
using boost::bind;

static bool
valid (Content const *)
{
	return true;
}

/** Player::overlaps */
BOOST_AUTO_TEST_CASE (player_overlaps_test)
{
	shared_ptr<Film> film = new_test_film ("player_overlaps_test");
	film->set_container (Ratio::from_id ("185"));

	/* This content is 3s long */
	shared_ptr<FFmpegContent> A (new FFmpegContent (film, "test/data/test.mp4"));
	shared_ptr<FFmpegContent> B (new FFmpegContent (film, "test/data/test.mp4"));
	shared_ptr<FFmpegContent> C (new FFmpegContent (film, "test/data/test.mp4"));

	film->examine_and_add_content (A);
	film->examine_and_add_content (B);
	film->examine_and_add_content (C);
	wait_for_jobs ();

	BOOST_CHECK_EQUAL (A->full_length().get(), 288000);

	A->set_position (DCPTime::from_seconds (0));
	B->set_position (DCPTime::from_seconds (10));
	C->set_position (DCPTime::from_seconds (20));

	shared_ptr<Player> player (new Player (film, film->playlist ()));

	list<shared_ptr<Piece> > o = player->overlaps (DCPTime::from_seconds (0), DCPTime::from_seconds (5), &valid);
	BOOST_CHECK_EQUAL (o.size(), 1U);
	BOOST_CHECK_EQUAL (o.front()->content, A);

	o = player->overlaps (DCPTime::from_seconds (5), DCPTime::from_seconds (8), &valid);
	BOOST_CHECK_EQUAL (o.size(), 0U);

	o = player->overlaps (DCPTime::from_seconds (8), DCPTime::from_seconds (12), &valid);
	BOOST_CHECK_EQUAL (o.size(), 1U);
	BOOST_CHECK_EQUAL (o.front()->content, B);

	o = player->overlaps (DCPTime::from_seconds (2), DCPTime::from_seconds (12), &valid);
	BOOST_CHECK_EQUAL (o.size(), 2U);
	BOOST_CHECK_EQUAL (o.front()->content, A);
	BOOST_CHECK_EQUAL (o.back()->content, B);

	o = player->overlaps (DCPTime::from_seconds (8), DCPTime::from_seconds (11), &valid);
	BOOST_CHECK_EQUAL (o.size(), 1U);
	BOOST_CHECK_EQUAL (o.front()->content, B);
}

static shared_ptr<AudioBuffers> accumulated;

static void
accumulate (shared_ptr<AudioBuffers> audio, DCPTime t)
{
	cout << "got " << audio->frames() << " @ " << to_string(t) << "\n";
	BOOST_REQUIRE (accumulated);
	accumulated->append (audio);
}

/** Check that the Player correctly generates silence when used with a silent FFmpegContent */
BOOST_AUTO_TEST_CASE (player_silence_padding_test)
{
	shared_ptr<Film> film = new_test_film ("player_silence_padding_test");
	film->set_name ("player_silence_padding_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	wait_for_jobs ();

	accumulated.reset (new AudioBuffers (film->audio_channels(), 0));

	shared_ptr<Player> player (new Player (film, film->playlist ()));
	player->Audio.connect (bind (&accumulate, _1, _2));
	while (!player->pass ()) {}
	BOOST_REQUIRE (accumulated->frames() >= 48000);
	BOOST_CHECK_EQUAL (accumulated->channels(), film->audio_channels ());

	for (int i = 0; i < 48000; ++i) {
		for (int c = 0; c < accumulated->channels(); ++c) {
			BOOST_CHECK_EQUAL (accumulated->data()[c][i], 0);
		}
	}
}

/* Test insertion of black frames between separate bits of video content */
BOOST_AUTO_TEST_CASE (player_black_fill_test)
{
	shared_ptr<Film> film = new_test_film ("black_fill_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("black_fill_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_sequence (false);
	shared_ptr<ImageContent> contentA (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent (film, "test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	wait_for_jobs ();

	contentA->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentA->video->set_length (3);
	contentA->set_position (DCPTime::from_frames (2, film->video_frame_rate ()));
	contentB->video->set_scale (VideoContentScale (Ratio::from_id ("185")));
	contentB->video->set_length (1);
	contentB->set_position (DCPTime::from_frames (7, film->video_frame_rate ()));

	film->make_dcp ();

	wait_for_jobs ();

	boost::filesystem::path ref;
	ref = "test";
	ref /= "data";
	ref /= "black_fill_test";

	boost::filesystem::path check;
	check = "build";
	check /= "test";
	check /= "black_fill_test";
	check /= film->dcp_name();

	check_dcp (ref.string(), check.string());
}

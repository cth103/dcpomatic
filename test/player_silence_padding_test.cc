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

/* @file  test/player_silence_padding_test.cc
 * @brief Check that the Player correctly generates silence when used with a silent FFmpegContent.
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
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (player_silence_padding_test)
{
	shared_ptr<Film> film = new_test_film ("player_silence_padding_test");
	film->set_name ("player_silence_padding_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);
	
	film->examine_and_add_content (c);
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


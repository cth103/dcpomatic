/*
    Copyright (C) 2017-2022 Carl Hetherington <cth@carlh.net>

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


#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/butler.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (butler_test1)
{
	auto film = new_test_film ("butler_test1");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("FTR"));
	film->set_name ("butler_test1");
	film->set_container (Ratio::from_id ("185"));

	auto video = content_factory("test/data/flat_red.png")[0];
	film->examine_and_add_content (video);
	auto audio = content_factory("test/data/staircase.wav")[0];
	film->examine_and_add_content (audio);
	BOOST_REQUIRE (!wait_for_jobs ());

	film->set_audio_channels (6);

	/* This is the map of the player output (5.1) to the butler output (also 5.1) */
	auto map = AudioMapping (6, 6);
	for (int i = 0; i < 6; ++i) {
		map.set (i, i, 1);
	}

	Player player(film, Image::Alignment::COMPACT);

	Butler butler (
		film,
		player,
		map,
		6,
		boost::bind(&PlayerVideo::force, AV_PIX_FMT_RGB24),
		VideoRange::FULL,
		Image::Alignment::COMPACT,
		false,
		false,
		Butler::Audio::ENABLED
		);

	BOOST_CHECK (butler.get_video(Butler::Behaviour::BLOCKING, 0).second == DCPTime());
	BOOST_CHECK (butler.get_video(Butler::Behaviour::BLOCKING, 0).second == DCPTime::from_frames(1, 24));
	BOOST_CHECK (butler.get_video(Butler::Behaviour::BLOCKING, 0).second == DCPTime::from_frames(2, 24));
	/* XXX: check the frame contents */

	float buffer[256 * 6];
	BOOST_REQUIRE (butler.get_audio(Butler::Behaviour::BLOCKING, buffer, 256) == DCPTime());
	for (int i = 0; i < 256; ++i) {
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 0], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 1], 0);
		BOOST_REQUIRE_CLOSE (buffer[i * 6 + 2], i / 32768.0f, 0.1);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 3], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 4], 0);
		BOOST_REQUIRE_EQUAL (buffer[i * 6 + 5], 0);
	}
}


BOOST_AUTO_TEST_CASE (butler_test2)
{
	auto content = content_factory(TestPaths::private_data() / "arrietty_JP-EN.mkv");
	BOOST_REQUIRE (!content.empty());
	auto film = new_test_film2 ("butler_test2", { content.front() });
	BOOST_REQUIRE (content.front()->audio);
	content.front()->audio->set_delay(100);

	/* This is the map of the player output (5.1) to the butler output (also 5.1) */
	auto map = AudioMapping (6, 6);
	for (int i = 0; i < 6; ++i) {
		map.set (i, i, 1);
	}

	Player player(film, Image::Alignment::COMPACT);

	Butler butler (
		film,
		player,
		map,
		6,
		boost::bind(&PlayerVideo::force, AV_PIX_FMT_RGB24),
		VideoRange::FULL,
		Image::Alignment::COMPACT,
		false,
		false,
		Butler::Audio::ENABLED
		);

	int const audio_frames_per_video_frame = 48000 / 25;
	float audio_buffer[audio_frames_per_video_frame * 6];
	for (int i = 0; i < 16; ++i) {
		butler.get_video(Butler::Behaviour::BLOCKING, 0);
		butler.get_audio(Butler::Behaviour::BLOCKING, audio_buffer, audio_frames_per_video_frame);
	}

	butler.seek (DCPTime::from_seconds(60), false);

	for (int i = 0; i < 240; ++i) {
		butler.get_video(Butler::Behaviour::BLOCKING, 0);
		butler.get_audio(Butler::Behaviour::BLOCKING, audio_buffer, audio_frames_per_video_frame);
	}

	butler.rethrow();
}


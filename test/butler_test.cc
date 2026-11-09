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
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <vector>


using std::make_shared;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (butler_test1)
{
	auto video = content_factory("test/data/flat_red.png")[0];
	auto audio = content_factory("test/data/staircase.wav")[0];
	auto film = new_test_film("butler_test1", { video, audio });
	film->set_audio_channels (6);

	/* This is the map of the player output (5.1) to the butler output (also 5.1) */
	auto map = AudioMapping (6, 6);
	for (int i = 0; i < 6; ++i) {
		map.set (i, i, 1);
	}

	Player player(film, Image::Alignment::COMPACT, false);

	Butler butler (
		film,
		player,
		map,
		6,
		AV_PIX_FMT_RGB24,
		VideoRange::FULL,
		Image::Alignment::COMPACT,
		false,
		false,
		Butler::Video::ENABLED,
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
	auto film = new_test_film("butler_test2", { content.front() });
	BOOST_REQUIRE (content.front()->audio);
	content.front()->audio->set_delay(100);

	/* This is the map of the player output (5.1) to the butler output (also 5.1) */
	auto map = AudioMapping (6, 6);
	for (int i = 0; i < 6; ++i) {
		map.set (i, i, 1);
	}

	Player player(film, Image::Alignment::COMPACT, false);

	Butler butler (
		film,
		player,
		map,
		6,
		AV_PIX_FMT_RGB24,
		VideoRange::FULL,
		Image::Alignment::COMPACT,
		false,
		false,
		Butler::Video::ENABLED,
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


/** @return the sum of the magnitudes of the samples of each butler output channel over 1 second of a film */
static std::vector<float>
butler_output_levels(std::shared_ptr<Film> film, AudioMapping map, int outputs)
{
	Player player(film, Image::Alignment::COMPACT, false);
	Butler butler(film, player, map, outputs, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::COMPACT, false, false, Butler::Video::ENABLED, Butler::Audio::ENABLED);

	int const frames_per_video_frame = film->audio_frame_rate() / film->video_frame_rate();
	std::vector<float> buffer(frames_per_video_frame * outputs);
	std::vector<float> levels(outputs, 0);
	for (int i = 0; i < film->video_frame_rate(); ++i) {
		butler.get_video(Butler::Behaviour::BLOCKING, 0);
		butler.get_audio(Butler::Behaviour::BLOCKING, buffer.data(), frames_per_video_frame);
		for (int frame = 0; frame < frames_per_video_frame; ++frame) {
			for (int channel = 0; channel < outputs; ++channel) {
				levels[channel] += std::abs(buffer[frame * outputs + channel]);
			}
		}
	}
	butler.rethrow();
	return levels;
}


/** Listening to HI or VI plays only that channel, and the main sound stops */
BOOST_AUTO_TEST_CASE(butler_listen_to_test)
{
	/* L.wav goes to L and C.wav to HI; nothing goes to the centre or to VI */
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto left = content_factory("test/data/L.wav")[0];
	auto hi = content_factory("test/data/C.wav")[0];
	auto film = new_test_film("butler_listen_to_test", { picture, left, hi });
	film->set_audio_channels(16);
	picture->video->set_length(24 * 5);

	auto left_mapping = left->audio->mapping();
	left_mapping.make_zero();
	left_mapping.set(0, dcp::Channel::LEFT, 1);
	left->audio->set_mapping(left_mapping);

	auto hi_mapping = hi->audio->mapping();
	hi_mapping.make_zero();
	hi_mapping.set(0, dcp::Channel::HI, 1);
	hi->audio->set_mapping(hi_mapping);

	auto const sound = [](float level) { return level > 100; };
	auto const silent = [](float level) { return level == 0; };

	/* The main mix to 5.1: only L has sound */
	AudioMapping main(MAX_DCP_AUDIO_CHANNELS, 6);
	main.make_zero();
	for (int i = 0; i < 6; ++i) {
		main.set(i, i, 1);
	}
	auto levels = butler_output_levels(film, main, 6);
	BOOST_CHECK(sound(levels[0]));
	for (int i = 1; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(silent(levels[i]), "main mix output " << i << " " << levels[i]);
	}

	/* HI to 5.1: only the centre has sound, and L is silent */
	levels = butler_output_levels(film, listen_to_mapping(dcp::Channel::HI, 6), 6);
	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(i == static_cast<int>(dcp::Channel::CENTRE) ? sound(levels[i]) : silent(levels[i]), "HI output " << i << " " << levels[i]);
	}

	/* HI to stereo: both outputs have sound */
	levels = butler_output_levels(film, listen_to_mapping(dcp::Channel::HI, 2), 2);
	BOOST_CHECK(sound(levels[0]));
	BOOST_CHECK(sound(levels[1]));

	/* VI, which has nothing mapped to it: silence */
	levels = butler_output_levels(film, listen_to_mapping(dcp::Channel::VI, 6), 6);
	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(silent(levels[i]), "VI output " << i << " " << levels[i]);
	}
}

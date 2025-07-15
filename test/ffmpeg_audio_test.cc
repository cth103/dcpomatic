/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/ffmpeg_audio_test.cc
 *  @brief Test reading audio from an FFmpeg file.
 *  @ingroup feature
 */


#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_frame.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/reel.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE (ffmpeg_audio_test)
{
	auto c = make_shared<FFmpegContent> ("test/data/staircase.mov");
	auto film = new_test_film("ffmpeg_audio_test", { c });

	int constexpr audio_channels = 6;

	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels(audio_channels);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	make_and_verify_dcp (film);

	boost::filesystem::path path = "build/test";
	path /= "ffmpeg_audio_test";
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	auto sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_REQUIRE_EQUAL(sound_asset->asset()->channels (), audio_channels);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	while (n < sound_asset->asset()->intrinsic_duration()) {
		auto sound_frame = sound_asset->asset()->start_read()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();
		for (int offset = 0; offset < sound_frame->size(); offset += (3 * sound_asset->asset()->channels())) {
			for (auto channel = 0; channel < audio_channels; ++channel) {
				auto const sample = d[offset + channel * 3 + 1] | (d[offset + channel * 3 + 2] << 8);
				if (channel == 2) {
					/* Input should be on centre */
					BOOST_CHECK_EQUAL(sample, n);
				} else {
					/* Everything else should be silent */
					BOOST_CHECK_EQUAL(sample, 0);
				}
			}
			++n;
		}
	}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to normal */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test2)
{
	auto film = new_test_film("ffmpeg_audio_test2");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	while (!player->pass ()) {}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to fast */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test3)
{
	auto film = new_test_film("ffmpeg_audio_test3");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	player->set_fast ();
	while (!player->pass ()) {}
}


/** Decode a file whose audio previously crashed DCP-o-matic (#1857) */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test4)
{
	auto film = new_test_film("ffmpeg_audio_test4");
	auto content = content_factory(TestPaths::private_data() / "Actuellement aout 2020.wmv")[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	player->set_fast ();
	BOOST_CHECK_NO_THROW (while (!player->pass()) {});
}



BOOST_AUTO_TEST_CASE(no_audio_length_in_header)
{
	auto content = content_factory(TestPaths::private_data() / "10-seconds.thd");
	auto film = new_test_film("no_audio_length_in_header", content);
	BOOST_CHECK(content[0]->full_length(film) == dcpomatic::DCPTime::from_seconds(10));
}

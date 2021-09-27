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


#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/video_content.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/player.h"
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
using std::shared_ptr;


BOOST_AUTO_TEST_CASE (ffmpeg_audio_test)
{
	auto film = new_test_film ("ffmpeg_audio_test");
	film->set_name ("ffmpeg_audio_test");
	auto c = make_shared<FFmpegContent> ("test/data/staircase.mov");
	film->examine_and_add_content (c);

	BOOST_REQUIRE (!wait_for_jobs());

	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	make_and_verify_dcp (film);

	boost::filesystem::path path = "build/test";
	path /= "ffmpeg_audio_test";
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	auto sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_CHECK_EQUAL (sound_asset->asset()->channels (), 6);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	while (n < sound_asset->asset()->intrinsic_duration()) {
		auto sound_frame = sound_asset->asset()->start_read()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();

		for (int i = 0; i < sound_frame->size(); i += (3 * sound_asset->asset()->channels())) {

			if (sound_asset->asset()->channels() > 0) {
				/* L should be silent */
				int const sample = d[i + 0] | (d[i + 1] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 1) {
				/* R should be silent */
				int const sample = d[i + 2] | (d[i + 3] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 2) {
				/* Mono input so it will appear on centre */
				int const sample = d[i + 7] | (d[i + 8] << 8);
				BOOST_CHECK_EQUAL (sample, n);
			}

			if (sound_asset->asset()->channels() > 3) {
				/* Lfe should be silent */
				int const sample = d[i + 9] | (d[i + 10] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 4) {
				/* Ls should be silent */
				int const sample = d[i + 11] | (d[i + 12] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}


			if (sound_asset->asset()->channels() > 5) {
				/* Rs should be silent */
				int const sample = d[i + 13] | (d[i + 14] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			++n;
		}
	}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to normal */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test2)
{
	auto film = new_test_film2 ("ffmpeg_audio_test2");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
	while (!player->pass ()) {}
}


/** Decode a file containing truehd so we can profile it; this is with the player set to fast */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test3)
{
	auto film = new_test_film2 ("ffmpeg_audio_test3");
	auto content = content_factory(TestPaths::private_data() / "wayne.mkv").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
	player->set_fast ();
	while (!player->pass ()) {}
}


/** Decode a file whose audio previously crashed DCP-o-matic (#1857) */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_test4)
{
	auto film = new_test_film2 ("ffmpeg_audio_test4");
	auto content = content_factory(TestPaths::private_data() / "Actuellement aout 2020.wmv").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs ());

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);
	player->set_fast ();
	BOOST_CHECK_NO_THROW (while (!player->pass()) {});
}


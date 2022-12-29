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


/** @defgroup feature Tests of features */

/** @file  test/audio_delay_test.cc
 *  @brief Test encode using some FFmpegContents which have audio delays.
 *  @ingroup feature
 *
 *  The output is checked algorithmically using knowledge of the input.
 */


#include "lib/audio_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::string;
using boost::lexical_cast;


static
void test_audio_delay (int delay_in_ms)
{
	string const film_name = "audio_delay_test_" + lexical_cast<string> (delay_in_ms);
	auto content = make_shared<FFmpegContent>("test/data/staircase.wav");
	auto film = new_test_film2 (film_name, { content });

	content->audio->set_delay (delay_in_ms);

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	boost::filesystem::path path = "build/test";
	path /= film_name;
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	auto sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;
	/* Delay in frames */
	int const delay_in_frames = delay_in_ms * 48000 / 1000;

	while (n < sound_asset->asset()->intrinsic_duration()) {
		auto sound_frame = sound_asset->asset()->start_read()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();

		for (int i = 0; i < sound_frame->size(); i += (3 * sound_asset->asset()->channels())) {

			/* Mono input so it will appear on centre */
			int const sample = d[i + 7] | (d[i + 8] << 8);

			int delayed = n - delay_in_frames;
			if (delayed < 0 || delayed >= 4800) {
				delayed = 0;
			}

			BOOST_REQUIRE_EQUAL (sample, delayed);
			++n;
		}
	}
}


/* Test audio delay when specified in a piece of audio content */
BOOST_AUTO_TEST_CASE (audio_delay_test)
{
	test_audio_delay (0);
	test_audio_delay (42);
	test_audio_delay (-66);
}

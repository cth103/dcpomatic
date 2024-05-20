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


/** @file  test/silence_padding_test.cc
 *  @brief Test the padding (with silence) of a mono source to a 6-channel DCP.
 *  @ingroup feature
 */


#include "lib/constants.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_frame.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;
using boost::lexical_cast;


static void
test_silence_padding(int channels, dcp::Standard standard)
{
	string const film_name = "silence_padding_test_" + lexical_cast<string> (channels);
	auto film = new_test_film(
		film_name,
		{
			make_shared<FFmpegContent>("test/data/flat_red.png"),
			make_shared<FFmpegContent>("test/data/staircase.wav")
		});

	if (standard == dcp::Standard::INTEROP) {
		film->set_interop(true);
	}
	film->set_audio_channels (channels);

	std::vector<dcp::VerificationNote::Code> codes;
	if (standard == dcp::Standard::INTEROP) {
		codes.push_back(dcp::VerificationNote::Code::INVALID_STANDARD);
	}
	auto const dcp_inspect = channels == 2 || channels == 6 || channels >= 8;
	auto const clairmeta = (channels % 2) == 0;
	make_and_verify_dcp(film, codes, dcp_inspect, clairmeta);

	boost::filesystem::path path = "build/test";
	path /= film_name;
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	auto sound_asset = check.cpls()[0]->reels()[0]->main_sound();
	BOOST_CHECK (sound_asset);
	BOOST_CHECK_EQUAL (sound_asset->asset()->channels (), channels);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	auto const asset_channels = sound_asset->asset()->channels();
	if (standard == dcp::Standard::SMPTE) {
		DCP_ASSERT(asset_channels == MAX_DCP_AUDIO_CHANNELS)
	} else {
		DCP_ASSERT(asset_channels == channels);
	}

	while (n < sound_asset->asset()->intrinsic_duration()) {
		auto sound_frame = sound_asset->asset()->start_read()->get_frame(frame++);
		uint8_t const * d = sound_frame->data ();

		for (int offset = 0; offset < sound_frame->size(); offset += (3 * asset_channels)) {

			for (auto channel = 0; channel < asset_channels; ++channel) {
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


BOOST_AUTO_TEST_CASE (silence_padding_test)
{
	for (int i = 1; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		test_silence_padding(i, dcp::Standard::INTEROP);
	}

	test_silence_padding(MAX_DCP_AUDIO_CHANNELS, dcp::Standard::SMPTE);
}


/** Test a situation that used to crash because of a sub-sample rounding confusion
 *  caused by a trim.
 */

BOOST_AUTO_TEST_CASE (silence_padding_test2)
{
	Cleanup cl;

	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "cars.mov");
	auto film = new_test_film("silence_padding_test2", { content }, &cl);

	film->set_video_frame_rate (24);
	content->set_trim_start(film, dcpomatic::ContentTime(4003));

	make_and_verify_dcp (film);

	cl.run ();
}

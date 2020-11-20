/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

using std::string;
using boost::lexical_cast;
using boost::shared_ptr;

static void
test_silence_padding (int channels)
{
	string const film_name = "silence_padding_test_" + lexical_cast<string> (channels);
	shared_ptr<Film> film = new_test_film (film_name);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name (film_name);

	shared_ptr<FFmpegContent> content (new FFmpegContent("test/data/staircase.wav"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_audio_channels (channels);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	boost::filesystem::path path = "build/test";
	path /= film_name;
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	shared_ptr<const dcp::ReelSoundAsset> sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_CHECK_EQUAL (sound_asset->asset()->channels (), channels);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	while (n < sound_asset->asset()->intrinsic_duration()) {
		shared_ptr<const dcp::SoundFrame> sound_frame = sound_asset->asset()->start_read()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();

		for (int i = 0; i < sound_frame->size(); i += (3 * sound_asset->asset()->channels())) {

			if (sound_asset->asset()->channels() > 0) {
				/* L should be silent */
				int const sample = d[i + 1] | (d[i + 2] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 1) {
				/* R should be silent */
				int const sample = d[i + 4] | (d[i + 5] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 2) {
				/* Mono input so it will appear on centre */
				int const sample = d[i + 7] | (d[i + 8] << 8);
				BOOST_CHECK_EQUAL (sample, n);
			}

			if (sound_asset->asset()->channels() > 3) {
				/* Lfe should be silent */
				int const sample = d[i + 10] | (d[i + 11] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->asset()->channels() > 4) {
				/* Ls should be silent */
				int const sample = d[i + 13] | (d[i + 14] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}


			if (sound_asset->asset()->channels() > 5) {
				/* Rs should be silent */
				int const sample = d[i + 16] | (d[i + 17] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			++n;
		}
	}

}

BOOST_AUTO_TEST_CASE (silence_padding_test)
{
	for (int i = 1; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		test_silence_padding (i);
	}
}

/** Test a situation that used to crash because of a sub-sample rounding confusion
 *  caused by a trim.
 */

BOOST_AUTO_TEST_CASE (silence_padding_test2)
{
	shared_ptr<Film> film = new_test_film2 ("silence_padding_test2");
	shared_ptr<FFmpegContent> content (new FFmpegContent(TestPaths::private_data() / "cars.mov"));
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_video_frame_rate (24);
	content->set_trim_start (dcpomatic::ContentTime(4003));

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

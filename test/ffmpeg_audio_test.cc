/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_audio_test.cc
 *  @brief A simple test of reading audio from an FFmpeg file.
 */

#include <boost/test/unit_test.hpp>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/sound_mxf.h>
#include <dcp/sound_frame.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel.h>
#include "lib/sndfile_content.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/ffmpeg_content.h"
#include "test.h"

using std::string;
using boost::lexical_cast;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_audio_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_audio_test");
	film->set_name ("ffmpeg_audio_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/staircase.mov"));
	film->examine_and_add_content (c);

	wait_for_jobs ();
	
	c->set_scale (VideoContentScale (Ratio::from_id ("185")));

	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->make_dcp ();
	film->write_metadata ();

	wait_for_jobs ();

	boost::filesystem::path path = "build/test";
	path /= "ffmpeg_audio_test";
	path /= film->dcp_name ();
	dcp::DCP check (path.string ());
	check.read ();

	shared_ptr<const dcp::ReelSoundAsset> sound_asset = check.cpls().front()->reels().front()->main_sound ();
	BOOST_CHECK (sound_asset);
	BOOST_CHECK_EQUAL (sound_asset->mxf()->channels (), 6);

	/* Sample index in the DCP */
	int n = 0;
	/* DCP sound asset frame */
	int frame = 0;

	while (n < sound_asset->mxf()->intrinsic_duration()) {
		shared_ptr<const dcp::SoundFrame> sound_frame = sound_asset->mxf()->get_frame (frame++);
		uint8_t const * d = sound_frame->data ();
		
		for (int i = 0; i < sound_frame->size(); i += (3 * sound_asset->mxf()->channels())) {

			if (sound_asset->mxf()->channels() > 0) {
				/* L should be silent */
				int const sample = d[i + 0] | (d[i + 1] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->mxf()->channels() > 1) {
				/* R should be silent */
				int const sample = d[i + 2] | (d[i + 3] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}
			
			if (sound_asset->mxf()->channels() > 2) {
				/* Mono input so it will appear on centre */
				int const sample = d[i + 7] | (d[i + 8] << 8);
				BOOST_CHECK_EQUAL (sample, n);
			}

			if (sound_asset->mxf()->channels() > 3) {
				/* Lfe should be silent */
				int const sample = d[i + 9] | (d[i + 10] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			if (sound_asset->mxf()->channels() > 4) {
				/* Ls should be silent */
				int const sample = d[i + 11] | (d[i + 12] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}


			if (sound_asset->mxf()->channels() > 5) {
				/* Rs should be silent */
				int const sample = d[i + 13] | (d[i + 14] << 8);
				BOOST_CHECK_EQUAL (sample, 0);
			}

			++n;
		}
	}
}

/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

/** @file  test/torture_test.cc
 *  @brief Entire projects that are programmatically created and checked.
 */

#include "lib/audio_content.h"
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/content_factory.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <boost/test/unit_test.hpp>

using std::list;
using boost::shared_ptr;

/** Test a fairly complex arrangement of content */
BOOST_AUTO_TEST_CASE (torture_test1)
{
	shared_ptr<Film> film = new_test_film ("player_torture_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("player_torture_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_sequence (false);

	/* Staircase at an offset of 2000 samples, trimmed both start and end, with a gain of 6dB */
	shared_ptr<Content> staircase = content_factory(film, "test/data/staircase.mov").front ();
	film->examine_and_add_content (staircase);
	wait_for_jobs ();
	staircase->set_position (DCPTime::from_frames (2000, film->audio_frame_rate()));
	staircase->set_trim_start (ContentTime::from_frames (12, 48000));
	staircase->set_trim_end (ContentTime::from_frames (35, 48000));
//	staircase->audio->set_gain (20 * log10(2));

	film->set_video_frame_rate (24);
	film->make_dcp ();
	wait_for_jobs ();

	dcp::DCP dcp ("build/test/player_torture_test/" + film->dcp_name(false));
	dcp.read ();

	list<shared_ptr<dcp::CPL> > cpls = dcp.cpls ();
	BOOST_REQUIRE_EQUAL (cpls.size(), 1);
	list<shared_ptr<dcp::Reel> > reels = cpls.front()->reels ();
	BOOST_REQUIRE_EQUAL (reels.size(), 1);

	shared_ptr<dcp::ReelSoundAsset> reel_sound = reels.front()->main_sound();
	BOOST_REQUIRE (reel_sound);
	shared_ptr<dcp::SoundAsset> sound = reel_sound->asset();
	BOOST_REQUIRE (sound);

	shared_ptr<dcp::SoundAssetReader> sound_reader = sound->start_read ();

	/* First frame silent */
	shared_ptr<const dcp::SoundFrame> fr = sound_reader->get_frame (0);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			BOOST_CHECK_EQUAL (fr->get(j, i), 0);
		}
	}

	/* The staircase is 4800 - 12 - 35 = 4753 samples.  One frame is 2000 samples, so we span 3 frames */

	BOOST_REQUIRE_EQUAL (fr->samples(), 2000);

	int stair = 12;

	fr = sound_reader->get_frame (1);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2) {
				BOOST_CHECK_EQUAL (fr->get(j, i) >> 8, stair);
				++stair;
			} else {
				BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, 0);
			}
		}
	}

	fr = sound_reader->get_frame (2);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2) {
				BOOST_CHECK_EQUAL (fr->get(j, i) >> 8, stair);
				++stair;
			} else {
				BOOST_CHECK_EQUAL (fr->get(j, i), 0);
			}
		}
	}

	fr = sound_reader->get_frame (3);
	for (int i = 0; i < 4753 - (2000 * 2); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2) {
				BOOST_CHECK_EQUAL (fr->get(j, i) >> 8, stair);
				++stair;
			} else {
				BOOST_CHECK_EQUAL (fr->get(j, i), 0);
			}
		}
	}
}

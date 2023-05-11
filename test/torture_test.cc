/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
 *  @brief Tricky arrangements of content whose resulting DCPs are checked programmatically.
 *  @ingroup completedcp
 */


#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using namespace dcpomatic;


/** Test start/end trim and positioning of some audio content */
BOOST_AUTO_TEST_CASE (torture_test1)
{
	auto film = new_test_film2 ("torture_test1");
	film->set_sequence (false);

	/* Staircase at an offset of 2000 samples, trimmed both start and end, with a gain of exactly 2 (linear) */
	auto staircase = content_factory("test/data/staircase.wav")[0];
	film->examine_and_add_content (staircase);
	BOOST_REQUIRE (!wait_for_jobs());
	staircase->set_position (film, DCPTime::from_frames(2000, film->audio_frame_rate()));
	staircase->set_trim_start(film, ContentTime::from_frames(12, 48000));
	staircase->set_trim_end (ContentTime::from_frames (35, 48000));
	staircase->audio->set_gain (20 * log10(2));

	/* And again at an offset of 50000 samples, trimmed both start and end, with a gain of exactly 2 (linear) */
	staircase = content_factory("test/data/staircase.wav")[0];
	film->examine_and_add_content (staircase);
	BOOST_REQUIRE (!wait_for_jobs());
	staircase->set_position (film, DCPTime::from_frames(50000, film->audio_frame_rate()));
	staircase->set_trim_start(film, ContentTime::from_frames(12, 48000));
	staircase->set_trim_end (ContentTime::from_frames(35, 48000));
	staircase->audio->set_gain (20 * log10(2));

	/* 1s of red at 5s in */
	auto red = content_factory("test/data/flat_red.png")[0];
	film->examine_and_add_content (red);
	BOOST_REQUIRE (!wait_for_jobs());
	red->set_position (film, DCPTime::from_seconds(5));
	red->video->set_length (24);

	film->set_video_frame_rate (24);
	make_and_verify_dcp (film);

	dcp::DCP dcp ("build/test/torture_test1/" + film->dcp_name(false));
	dcp.read ();

	auto cpls = dcp.cpls ();
	BOOST_REQUIRE_EQUAL (cpls.size(), 1U);
	auto reels = cpls.front()->reels ();
	BOOST_REQUIRE_EQUAL (reels.size(), 1U);

	/* Check sound */

	auto reel_sound = reels.front()->main_sound();
	BOOST_REQUIRE (reel_sound);
	auto sound = reel_sound->asset();
	BOOST_REQUIRE (sound);
	BOOST_CHECK_EQUAL (sound->intrinsic_duration(), 144);

	auto sound_reader = sound->start_read ();

	/* First frame silent */
	auto fr = sound_reader->get_frame (0);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			BOOST_CHECK_EQUAL (fr->get(j, i), 0);
		}
	}

	/* The first staircase is 4800 - 12 - 35 = 4753 samples.  One frame is 2000 samples, so we span 3 frames */

	BOOST_REQUIRE_EQUAL (fr->samples(), 2000);

	int stair = 12;

	BOOST_TEST_CONTEXT("First staircase, frame #1") {
		fr = sound_reader->get_frame (1);
		for (int i = 0; i < fr->samples(); ++i) {
			for (int j = 0; j < 6; ++j) {
				if (j == 2) {
					BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
					++stair;
				} else {
					BOOST_CHECK_EQUAL (fr->get(j, i), 0);
				}
			}
		}
	}

	BOOST_TEST_CONTEXT("First staircase, frame #2") {
		fr = sound_reader->get_frame (2);
		for (int i = 0; i < fr->samples(); ++i) {
			for (int j = 0; j < 6; ++j) {
				if (j == 2) {
					BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
					++stair;
				} else {
					BOOST_CHECK_EQUAL (fr->get(j, i), 0);
				}
			}
		}
	}

	BOOST_TEST_CONTEXT("First staircase, frame #3") {
		fr = sound_reader->get_frame (3);
		for (int i = 0; i < fr->samples(); ++i) {
			for (int j = 0; j < 6; ++j) {
				if (j == 2 && i < (4753 - (2000 * 2))) {
					BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
					++stair;
				} else {
					BOOST_CHECK_EQUAL (fr->get(j, i), 0);
				}
			}
		}
	}

	/* Then some silence */

	BOOST_TEST_CONTEXT("Silence") {
		for (int i = 4; i < 24; ++i) {
			fr = sound_reader->get_frame (i);
			for (int j = 0; j < fr->samples(); ++j) {
				for (int k = 0; k < 6; ++k) {
					BOOST_CHECK_EQUAL (fr->get(k, j), 0);
				}
			}
		}
	}

	/* Then the same thing again */

	stair = 12;

	fr = sound_reader->get_frame (25);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2) {
				BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
				++stair;
			} else {
				BOOST_CHECK_EQUAL (fr->get(j, i), 0);
			}
		}
	}

	fr = sound_reader->get_frame (26);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2) {
				BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
				++stair;
			} else {
				BOOST_CHECK_EQUAL (fr->get(j, i), 0);
			}
		}
	}

	fr = sound_reader->get_frame (27);
	for (int i = 0; i < fr->samples(); ++i) {
		for (int j = 0; j < 6; ++j) {
			if (j == 2 && i < (4753 - (2000 * 2))) {
				BOOST_CHECK_EQUAL ((fr->get(j, i) + 128) >> 8, stair * 2);
				++stair;
			} else {
				BOOST_CHECK_EQUAL (fr->get(j, i), 0);
			}
		}
	}

	/* Then some silence */

	for (int i = 28; i < 144; ++i) {
		fr = sound_reader->get_frame (i);
		for (int j = 0; j < fr->samples(); ++j) {
			for (int k = 0; k < 6; ++k) {
				BOOST_CHECK_EQUAL (fr->get(k, j), 0);
			}
		}
	}

	/* Check picture */

	auto reel_picture = reels.front()->main_picture();
	BOOST_REQUIRE (reel_picture);
	auto picture = dynamic_pointer_cast<dcp::MonoPictureAsset> (reel_picture->asset());
	BOOST_REQUIRE (picture);
	BOOST_CHECK_EQUAL (picture->intrinsic_duration(), 144);

	auto picture_reader = picture->start_read ();

	/* First 5 * 24 = 120 frames should be black, possibly with a little noise to raise the bitrate */

	shared_ptr<dcp::OpenJPEGImage> ref;
	for (int i = 0; i < 120; ++i) {
		auto fr = picture_reader->get_frame (i);
		auto image = fr->xyz_image ();
		auto const size = image->size ();
		if (i == 0) {
			/* Check the first frame pixel by pixel... */
			for (int c = 0; c < 3; ++c) {
				for (int y = 0; y < size.height; ++y) {
					for (int x = 0; x < size.width; ++x) {
						BOOST_REQUIRE (image->data(c)[y * size.height + x] <= 3);
					}
				}
			}
			ref = image;
		} else {
			/* ... then all the others should be the same */
			for (int c = 0; c < 3; ++c) {
				BOOST_REQUIRE_MESSAGE (
					memcmp (image->data(c), ref->data(c), size.width * size.height * sizeof(int)) == 0,
					"failed on frame " << i << " component " << c
					);
			}
		}
	}

	/* Then 24 red, perhaps also with some noise */

	for (int i = 120; i < 144; ++i) {
		auto fr = picture_reader->get_frame (i);
		auto image = fr->xyz_image ();
		auto const size = image->size ();
		if (i == 120) {
			for (int y = 0; y < size.height; ++y) {
				for (int x = 0; x < size.width; ++x) {
					BOOST_REQUIRE_MESSAGE (
						abs(image->data(0)[y * size.height + x] - 2808) <= 5,
						"failed on frame " << i << " with image data " << image->data(0)[y * size.height + x]
						);
					BOOST_REQUIRE_MESSAGE (
						abs(image->data(1)[y * size.height + x] - 2176) <= 5,
						"failed on frame " << i << " with image data " << image->data(1)[y * size.height + x]
						);
					BOOST_REQUIRE_MESSAGE (
						abs(image->data(2)[y * size.height + x] - 865) <= 5,
						"failed on frame " << i << " with image data " << image->data(2)[y * size.height + x]
						);
				}
			}
			ref = image;
		} else {
			for (int c = 0; c < 3; ++c) {
				BOOST_REQUIRE_MESSAGE (
					memcmp (image->data(c), ref->data(c), size.width * size.height * sizeof(int)) == 0,
					"failed on frame " << i << " component " << c
					);
			}
		}
	}

}

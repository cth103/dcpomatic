/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_audio_only_test.cc
 *  @brief Test FFmpeg content with audio but no video.
 *  @ingroup specific
 */

#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/player.h"
#include "lib/job_manager.h"
#include "lib/audio_buffers.h"
#include "test.h"
#include <sndfile.h>
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

static SNDFILE* ref = 0;
static int ref_buffer_size = 0;
static float* ref_buffer = 0;

static void
audio (boost::shared_ptr<AudioBuffers> audio, int channels)
{
	/* Check that we have a big enough buffer */
	BOOST_CHECK (audio->frames() * audio->channels() < ref_buffer_size);

	int const N = sf_readf_float (ref, ref_buffer, audio->frames());
	for (int i = 0; i < N; ++i) {
		switch (channels) {
		case 1:
			BOOST_CHECK_EQUAL (ref_buffer[i], audio->data(2)[i]);
			break;
		case 2:
			BOOST_CHECK_EQUAL (ref_buffer[i*2 + 0], audio->data(0)[i]);
			BOOST_CHECK_EQUAL (ref_buffer[i*2 + 1], audio->data(1)[i]);
			break;
		default:
			BOOST_REQUIRE (false);
		}
	}
}

/** Test the FFmpeg code with audio-only content */
static void
test (boost::filesystem::path file)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_audio_only_test");
	film->set_name ("test_film");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, file));
	film->examine_and_add_content (c);
	wait_for_jobs ();
	film->write_metadata ();

	/* See if can make a DCP without any errors */
	film->make_dcp ();
	wait_for_jobs ();
	BOOST_CHECK (!JobManager::instance()->errors());

	/* Compare the audio data we read with what libsndfile reads */

	SF_INFO info;
	info.format = 0;
	ref = sf_open (file.string().c_str(), SFM_READ, &info);
	/* We don't want to test anything that requires resampling */
	BOOST_REQUIRE_EQUAL (info.samplerate, 48000);
	ref_buffer_size = info.samplerate * info.channels;
	ref_buffer = new float[ref_buffer_size];

	shared_ptr<Player> player (new Player (film, film->playlist ()));

	player->Audio.connect (bind (&audio, _1, info.channels));
	while (!player->pass ()) {}

	sf_close (ref);
}

BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test1)
{
	/* S16 */
	test ("test/data/staircase.wav");
}

BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test2)
{
	/* S32 1 channel */
	test ("test/data/sine_440.wav");
}

/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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
 *  @ingroup feature
 */


#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/player.h"
#include "lib/job_manager.h"
#include "lib/audio_buffers.h"
#include "test.h"
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <sndfile.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::min;
using std::shared_ptr;
using std::make_shared;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static SNDFILE* ref = nullptr;
static std::vector<float> ref_buffer;


static void
audio (std::shared_ptr<AudioBuffers> audio, int channels)
{
	/* Check that we have a big enough buffer */
	BOOST_CHECK (audio->frames() * audio->channels() < static_cast<int>(ref_buffer.size()));

	int const N = sf_readf_float (ref, ref_buffer.data(), audio->frames());
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
static shared_ptr<Film>
test (boost::filesystem::path file)
{
	auto film = new_test_film ("ffmpeg_audio_only_test");
	film->set_name ("test_film");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	auto c = make_shared<FFmpegContent>(file);
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());
	film->write_metadata ();

	/* See if can make a DCP without any errors */
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});
	BOOST_CHECK (!JobManager::instance()->errors());

	/* Compare the audio data player reads with what libsndfile reads */

	SF_INFO info;
	info.format = 0;
	ref = sf_open (file.string().c_str(), SFM_READ, &info);
	/* We don't want to test anything that requires resampling */
	BOOST_REQUIRE_EQUAL (info.samplerate, 48000);
	ref_buffer.resize(info.samplerate * info.channels);

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT);

	player->Audio.connect (bind (&audio, _1, info.channels));
	while (!player->pass ()) {}

	sf_close (ref);

	return film;
}


BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test1)
{
	/* S16 */
	auto film = test ("test/data/staircase.wav");

	/* Compare the audio data in the DCP with what libsndfile reads */

	SF_INFO info;
	info.format = 0;
	ref = sf_open ("test/data/staircase.wav", SFM_READ, &info);
	/* We don't want to test anything that requires resampling */
	BOOST_REQUIRE_EQUAL (info.samplerate, 48000);

	std::vector<int16_t> buffer(info.channels * 2000);

	dcp::SoundAsset asset (dcp_file(film, "pcm"));
	auto reader = asset.start_read ();
	for (int i = 0; i < asset.intrinsic_duration(); ++i) {
		auto frame = reader->get_frame(i);
		sf_count_t this_time = min (info.frames, sf_count_t(2000));
		sf_readf_short (ref, buffer.data(), this_time);
		for (int j = 0; j < this_time; ++j) {
			BOOST_REQUIRE_EQUAL (frame->get(2, j) >> 8, buffer[j]);
		}
		info.frames -= this_time;
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test2)
{
	/* S32 1 channel */
	auto film = test ("test/data/sine_440.wav");

	/* Compare the audio data in the DCP with what libsndfile reads */

	SF_INFO info;
	info.format = 0;
	ref = sf_open ("test/data/sine_440.wav", SFM_READ, &info);
	/* We don't want to test anything that requires resampling */
	BOOST_REQUIRE_EQUAL (info.samplerate, 48000);

	std::vector<int32_t> buffer(info.channels * 2000);

	dcp::SoundAsset asset (dcp_file(film, "pcm"));
	auto reader = asset.start_read ();
	for (int i = 0; i < asset.intrinsic_duration(); ++i) {
		auto frame = reader->get_frame(i);
		sf_count_t this_time = min (info.frames, sf_count_t(2000));
		sf_readf_int (ref, buffer.data(), this_time);
		for (int j = 0; j < this_time; ++j) {
			int32_t s = frame->get(2, j);
			if (s > (1 << 23)) {
				s -= (1 << 24);
			}
			BOOST_REQUIRE_MESSAGE (abs(s - (buffer[j] / 256)) <= 1, "failed on asset frame " << i << " sample " << j);
		}
		info.frames -= this_time;
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test3)
{
	/* S24 1 channel */
	auto film = test ("test/data/sine_24_48_440.wav");

	/* Compare the audio data in the DCP with what libsndfile reads */

	SF_INFO info;
	info.format = 0;
	ref = sf_open ("test/data/sine_24_48_440.wav", SFM_READ, &info);
	/* We don't want to test anything that requires resampling */
	BOOST_REQUIRE_EQUAL (info.samplerate, 48000);

	std::vector<int32_t> buffer(info.channels * 2000);

	dcp::SoundAsset asset (dcp_file(film, "pcm"));
	auto reader = asset.start_read ();
	for (int i = 0; i < asset.intrinsic_duration(); ++i) {
		auto frame = reader->get_frame(i);
		sf_count_t this_time = min (info.frames, sf_count_t(2000));
		sf_readf_int (ref, buffer.data(), this_time);
		for (int j = 0; j < this_time; ++j) {
			int32_t s = frame->get(2, j);
			if (s > (1 << 23)) {
				s -= (1 << 24);
			}
			BOOST_REQUIRE_MESSAGE (abs(s - buffer[j] /256) <= 1, "failed on asset frame " << i << " sample " << j);
		}
		info.frames -= this_time;
	}
}

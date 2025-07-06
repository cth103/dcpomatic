/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/audio_processor_test.cc
 *  @brief Test audio processors.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/audio_processor.h"
#include "lib/analyse_audio_job.h"
#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/job_manager.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/sound_asset.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;


/** Test the mid-side decoder for analysis and DCP-making */
BOOST_AUTO_TEST_CASE (audio_processor_test)
{
	auto c = make_shared<FFmpegContent>("test/data/white.wav");
	auto film = new_test_film("audio_processor_test", { c });

	film->set_audio_channels(16);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_audio_processor (AudioProcessor::from_id ("mid-side-decoder"));

	/* Analyse the audio and check it doesn't crash */
	auto job = make_shared<AnalyseAudioJob> (film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make a DCP and check it */
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});
	check_dcp ("test/data/audio_processor_test", film->dir (film->dcp_name ()));
}



/** Check that it's possible to pass data through the mid-side decoder via HI/VI etc. */
BOOST_AUTO_TEST_CASE(audio_processor_pass_through_test)
{
	auto staircase = content_factory("test/data/staircase.wav")[0];

	auto film = new_test_film("audio_processor_pass_through_test", { staircase });
	film->set_audio_channels(16);
	film->set_audio_processor(AudioProcessor::from_id("mid-side-decoder"));

	AudioMapping mapping(1, 16);
	mapping.set(0, dcp::Channel::HI, 1);
	mapping.set(0, dcp::Channel::VI, 1);
	mapping.set(0, dcp::Channel::MOTION_DATA, 1);
	mapping.set(0, dcp::Channel::SYNC_SIGNAL, 1);
	mapping.set(0, dcp::Channel::SIGN_LANGUAGE, 1);
	staircase->audio->set_mapping(mapping);

	make_and_verify_dcp(film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});

	auto mxf = find_file(film->dir(film->dcp_name()), "pcm_");
	dcp::SoundAsset asset(mxf);
	auto reader = asset.start_read();
	auto frame = reader->get_frame(0);
	BOOST_REQUIRE(frame);

	/* Check the first 512 samples */
	for (auto i = 0; i < 512; ++i) {
		BOOST_REQUIRE_EQUAL(frame->get(0, i), 0); // L
		BOOST_REQUIRE_EQUAL(frame->get(1, i), 0); // R
		BOOST_REQUIRE_EQUAL(frame->get(2, i), 0); // C
		BOOST_REQUIRE_EQUAL(frame->get(3, i), 0); // Lfe
		BOOST_REQUIRE_EQUAL(frame->get(4, i), 0); // Ls
		BOOST_REQUIRE_EQUAL(frame->get(5, i), 0); // Rs
		BOOST_REQUIRE_EQUAL(frame->get(6, i), i << 8); // HI
		BOOST_REQUIRE_EQUAL(frame->get(7, i), i << 8); // VI
		BOOST_REQUIRE_EQUAL(frame->get(8, i), 0); // LC
		BOOST_REQUIRE_EQUAL(frame->get(9, i), 0); // RC
		BOOST_REQUIRE_EQUAL(frame->get(10, i), 0); // BSL
		BOOST_REQUIRE_EQUAL(frame->get(11, i), 0); // BSR
		BOOST_REQUIRE_EQUAL(frame->get(12, i), i << 8); // Motion data
		BOOST_REQUIRE_EQUAL(frame->get(13, i), i << 8); // Sync
		BOOST_REQUIRE_EQUAL(frame->get(14, i), i << 8); // Sign language
		BOOST_REQUIRE_EQUAL(frame->get(15, i), 0); // Unused
	}
}


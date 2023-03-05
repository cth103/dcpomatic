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


/** @file  test/threed_test.cc
 *  @brief Create some 3D DCPs (without comparing the results to anything).
 *  @ingroup completedcp
 */


#include "lib/butler.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/make_dcp.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::shared_ptr;


/** Basic sanity check of THREE_D_LEFT_RIGHT */
BOOST_AUTO_TEST_CASE (threed_test1)
{
	auto film = new_test_film ("threed_test1");
	film->set_name ("test_film1");
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D_LEFT_RIGHT);

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	make_and_verify_dcp (film);
}


/** Basic sanity check of THREE_D_ALTERNATE; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test2)
{
	auto film = new_test_film ("threed_test2");
	film->set_name ("test_film2");
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D_ALTERNATE);

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_three_d (true);
	make_and_verify_dcp (film);
}


/** Basic sanity check of THREE_D_LEFT and THREE_D_RIGHT; at the moment this is just to make sure
 *  that such a transcode completes without error.
 */
BOOST_AUTO_TEST_CASE (threed_test3)
{
	auto film = new_test_film2 ("threed_test3");
	auto L = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);

	film->set_three_d (true);
	make_and_verify_dcp (film);
}


BOOST_AUTO_TEST_CASE (threed_test4)
{
	ConfigRestorer cr;

	/* Try to stop out-of-memory crashes on my laptop */
	Config::instance()->set_master_encoding_threads (boost::thread::hardware_concurrency() / 4);

	auto film = new_test_film2 ("threed_test4");
	auto L = make_shared<FFmpegContent>(TestPaths::private_data() / "LEFT_TEST_DCP3D4K.mov");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>(TestPaths::private_data() / "RIGHT_TEST_DCP3D4K.mov");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	/* There doesn't seem much point in encoding the whole input, especially as we're only
	 * checking for errors during the encode and not the result.  Also decoding these files
	 * (4K HQ Prores) is very slow.
	 */
	L->set_trim_end (dcpomatic::ContentTime::from_seconds(22));
	R->set_trim_end (dcpomatic::ContentTime::from_seconds(22));

	film->set_three_d (true);
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_PICTURE_ASSET_RESOLUTION_FOR_3D});
}


BOOST_AUTO_TEST_CASE (threed_test5)
{
	auto film = new_test_film2 ("threed_test5");
	auto L = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>(TestPaths::private_data() / "boon_telly.mkv");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	/* There doesn't seem much point in encoding the whole input, especially as we're only
	 * checking for errors during the encode and not the result.
	 */
	L->set_trim_end (dcpomatic::ContentTime::from_seconds(3 * 60 + 20));
	R->set_trim_end (dcpomatic::ContentTime::from_seconds(3 * 60 + 20));

	film->set_three_d (true);
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K});
}


BOOST_AUTO_TEST_CASE (threed_test6)
{
	auto film = new_test_film2 ("threed_test6");
	auto L = make_shared<FFmpegContent>("test/data/3dL.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/3dR.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);

	film->set_three_d (true);
	make_and_verify_dcp (film);
	check_dcp ("test/data/threed_test6", film->dir(film->dcp_name()));
}


/** Check 2D content set as being 3D; this should give an informative error */
BOOST_AUTO_TEST_CASE (threed_test7)
{
	using boost::filesystem::path;

	auto film = new_test_film2 ("threed_test7");
	path const content_path = "test/data/flat_red.png";
	auto c = content_factory(content_path)[0];
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	c->video->set_frame_type (VideoFrameType::THREE_D);
	c->video->set_length (24);

	film->set_three_d (true);
	make_dcp (film, TranscodeJob::ChangedBehaviour::IGNORE);
	film->write_metadata ();

	auto jm = JobManager::instance ();
	while (jm->work_to_do ()) {
		while (signal_manager->ui_idle()) {}
		dcpomatic_sleep_seconds (1);
	}

	while (signal_manager->ui_idle ()) {}

	BOOST_REQUIRE (jm->errors());
	shared_ptr<Job> failed;
	for (auto i: jm->_jobs) {
		if (i->finished_in_error()) {
			BOOST_REQUIRE (!failed);
			failed = i;
		}
	}
	BOOST_REQUIRE (failed);
	BOOST_CHECK_EQUAL (failed->error_summary(), String::compose("The content file %1 is set as 3D but does not appear to contain 3D images.  Please set it to 2D.  You can still make a 3D DCP from this content by ticking the 3D option in the DCP video tab.", content_path.string()));

	while (signal_manager->ui_idle ()) {}

	JobManager::drop ();
}


/** Trigger a -114 error by trying to make a 3D DCP out of two files with slightly
 *  different lengths.
 */
BOOST_AUTO_TEST_CASE (threed_test_separate_files_slightly_different_lengths)
{
	auto film = new_test_film2("threed_test3");
	auto L = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	R->set_trim_end (dcpomatic::ContentTime::from_frames(1, 24));

	film->set_three_d (true);
	make_and_verify_dcp (film);
}


/** Trigger a -114 error by trying to make a 3D DCP out of two files with very
 *  different lengths.
 */
BOOST_AUTO_TEST_CASE (threed_test_separate_files_very_different_lengths)
{
	auto film = new_test_film2("threed_test3");
	auto L = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (L);
	auto R = make_shared<FFmpegContent>("test/data/test.mp4");
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());

	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	R->set_trim_end (dcpomatic::ContentTime::from_seconds(1.5));

	film->set_three_d (true);
	make_and_verify_dcp (film);
}


BOOST_AUTO_TEST_CASE (threed_test_butler_overfill)
{
	auto film = new_test_film2("threed_test_butler_overfill");
	auto A = make_shared<FFmpegContent>(TestPaths::private_data() / "arrietty_JP-EN.mkv");
	film->examine_and_add_content(A);
	auto B = make_shared<FFmpegContent>(TestPaths::private_data() / "arrietty_JP-EN.mkv");
	film->examine_and_add_content(B);
	BOOST_REQUIRE (!wait_for_jobs());

	Player player(film, Image::Alignment::COMPACT);
	int const audio_channels = 2;
	auto butler = std::make_shared<Butler>(
		film, player, AudioMapping(), audio_channels, boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::ENABLED
		);

	int const audio_frames = 1920;
	std::vector<float> audio(audio_frames * audio_channels);

	B->video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
	B->set_position(film, dcpomatic::DCPTime());

	butler->seek(dcpomatic::DCPTime(), true);
	Butler::Error error;
	for (auto i = 0; i < 960; ++i) {
		butler->get_video(Butler::Behaviour::BLOCKING, &error);
		butler->get_audio(Butler::Behaviour::BLOCKING, audio.data(), audio_frames);
	}
	BOOST_REQUIRE (error.code == Butler::Error::Code::NONE);
}


/** Check that creating a 2D DCP from a 3D DCP passes the J2K data unaltered */
BOOST_AUTO_TEST_CASE(threed_passthrough_test, * boost::unit_test::depends_on("threed_test6"))
{
	using namespace boost::filesystem;

	/* Find the DCP in threed_test6 */
	boost::optional<path> input_dcp;
	for (auto i: directory_iterator("build/test/threed_test6")) {
		if (is_directory(i.path()) && boost::algorithm::starts_with(i.path().filename().string(), "Dcp")) {
			input_dcp = i.path();
		}
	}

	BOOST_REQUIRE(input_dcp);

	auto content = make_shared<DCPContent>(*input_dcp);
	auto film = new_test_film2("threed_passthrough_test", { content });
	film->set_three_d(false);

	make_and_verify_dcp(film);

	std::vector<directory_entry> matches;
	std::copy_if(recursive_directory_iterator(*input_dcp), recursive_directory_iterator(), std::back_inserter(matches), [](directory_entry const& entry) {
		return boost::algorithm::starts_with(entry.path().leaf().string(), "j2c");
	});

	BOOST_REQUIRE_EQUAL(matches.size(), 1U);

	auto stereo = dcp::StereoPictureAsset(matches[0]);
	auto stereo_reader = stereo.start_read();

	auto mono = dcp::MonoPictureAsset(dcp_file(film, "j2c"));
	auto mono_reader = mono.start_read();

	BOOST_REQUIRE_EQUAL(stereo.intrinsic_duration(), mono.intrinsic_duration());

	for (auto i = 0; i < stereo.intrinsic_duration(); ++i) {
		auto stereo_frame = stereo_reader->get_frame(i);
		auto mono_frame = mono_reader->get_frame(i);
		BOOST_REQUIRE(stereo_frame->left()->size() == mono_frame->size());
		BOOST_REQUIRE_EQUAL(memcmp(stereo_frame->left()->data(), mono_frame->data(), mono_frame->size()), 0);
	}
}

/* #2476 was a writer error when 3D picture padding is needed */
BOOST_AUTO_TEST_CASE(threed_test_when_padding_needed)
{
	auto left = content_factory("test/data/flat_red.png").front();
	auto right = content_factory("test/data/flat_red.png").front();
	auto sound = content_factory("test/data/sine_440.wav").front();
	auto film = new_test_film2("threed_test_when_padding_needed", { left, right, sound });

	left->video->set_frame_type(VideoFrameType::THREE_D_LEFT);
	left->set_position(film, dcpomatic::DCPTime());
	left->video->set_length(23);
	right->video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
	right->set_position(film, dcpomatic::DCPTime());
	right->video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
	right->video->set_length(23);
	film->set_three_d(true);

	make_and_verify_dcp(film);
}

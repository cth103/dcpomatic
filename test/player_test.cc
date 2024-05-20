/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/player_test.cc
 *  @brief Test Player class.
 *  @ingroup selfcontained
 */


#include "lib/audio_buffers.h"
#include "lib/audio_content.h"
#include "lib/butler.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcpomatic_log.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/image_png.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>


using std::cout;
using std::list;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static shared_ptr<AudioBuffers> accumulated;


static void
accumulate (shared_ptr<AudioBuffers> audio, DCPTime)
{
	BOOST_REQUIRE (accumulated);
	accumulated->append (audio);
}


/** Check that the Player correctly generates silence when used with a silent FFmpegContent */
BOOST_AUTO_TEST_CASE (player_silence_padding_test)
{
	auto c = std::make_shared<FFmpegContent>("test/data/test.mp4");
	auto film = new_test_film2("player_silence_padding_test", { c });
	film->set_audio_channels (6);

	accumulated = std::make_shared<AudioBuffers>(film->audio_channels(), 0);

	Player player(film, Image::Alignment::COMPACT);
	player.Audio.connect(bind(&accumulate, _1, _2));
	while (!player.pass()) {}
	BOOST_REQUIRE (accumulated->frames() >= 48000);
	BOOST_CHECK_EQUAL (accumulated->channels(), film->audio_channels ());

	for (int i = 0; i < 48000; ++i) {
		for (int c = 0; c < accumulated->channels(); ++c) {
			BOOST_CHECK_EQUAL (accumulated->data()[c][i], 0);
		}
	}
}


/* Test insertion of black frames between separate bits of video content */
BOOST_AUTO_TEST_CASE (player_black_fill_test)
{
	auto contentA = std::make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = std::make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto film = new_test_film2("black_fill_test", { contentA, contentB });
	film->set_dcp_content_type(DCPContentType::from_isdcf_name("FTR"));
	film->set_sequence (false);

	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime::from_frames(2, film->video_frame_rate()));
	contentA->video->set_custom_ratio (1.85);
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, film->video_frame_rate()));
	contentB->video->set_custom_ratio (1.85);

	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE
		});

	boost::filesystem::path ref;
	ref = "test";
	ref /= "data";
	ref /= "black_fill_test";

	boost::filesystem::path check;
	check = "build";
	check /= "test";
	check /= "black_fill_test";
	check /= film->dcp_name();

	/* This test is concerned with the image, so we'll ignore any
	 * differences in sound between the DCP and the reference to avoid test
	 * failures for unrelated reasons.
	 */
	check_dcp(ref.string(), check.string(), true);
}


/** Check behaviour with an awkward playlist whose data does not end on a video frame start */
BOOST_AUTO_TEST_CASE (player_subframe_test)
{
	auto A = content_factory("test/data/flat_red.png")[0];
	auto B = content_factory("test/data/awkward_length.wav")[0];
	auto film = new_test_film2("reels_test7", { A, B });
	film->set_video_frame_rate (24);
	A->video->set_length (3 * 24);

	BOOST_CHECK (A->full_length(film) == DCPTime::from_frames(3 * 24, 24));
	BOOST_CHECK (B->full_length(film) == DCPTime(289920));
	/* Length should be rounded up from B's length to the next video frame */
	BOOST_CHECK (film->length() == DCPTime::from_frames(3 * 24 + 1, 24));

	Player player(film, Image::Alignment::COMPACT);
	player.setup_pieces();
	BOOST_REQUIRE_EQUAL(player._black._periods.size(), 1U);
	BOOST_CHECK(player._black._periods.front() == DCPTimePeriod(DCPTime::from_frames(3 * 24, 24), DCPTime::from_frames(3 * 24 + 1, 24)));
	BOOST_REQUIRE_EQUAL(player._silent._periods.size(), 1U);
	BOOST_CHECK(player._silent._periods.front() == DCPTimePeriod(DCPTime(289920), DCPTime::from_frames(3 * 24 + 1, 24)));
}


static Frame video_frames;
static Frame audio_frames;


static void
video (shared_ptr<PlayerVideo>, DCPTime)
{
	++video_frames;
}

static void
audio (shared_ptr<AudioBuffers> audio, DCPTime)
{
	audio_frames += audio->frames();
}


/** Check with a video-only file that the video and audio emissions happen more-or-less together */
BOOST_AUTO_TEST_CASE (player_interleave_test)
{
	auto c = std::make_shared<FFmpegContent>("test/data/test.mp4");
	auto s = std::make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film2("ffmpeg_transcoder_basic_test_subs", { c, s });
	film->set_audio_channels (6);

	Player player(film, Image::Alignment::COMPACT);
	player.Video.connect(bind(&video, _1, _2));
	player.Audio.connect(bind(&audio, _1, _2));
	video_frames = audio_frames = 0;
	while (!player.pass()) {
		BOOST_CHECK (abs(video_frames - (audio_frames / 2000)) <= 8);
	}
}


/** Test some seeks towards the start of a DCP with awkward subtitles; see mantis #1085
 *  and a number of others.  I thought this was a player seek bug but in fact it was
 *  caused by the subtitle starting just after the start of the video frame and hence
 *  being faded out.
 */
BOOST_AUTO_TEST_CASE (player_seek_test)
{
	auto film = std::make_shared<Film>(optional<boost::filesystem::path>());
	auto dcp = std::make_shared<DCPContent>(TestPaths::private_data() / "awkward_subs");
	film->examine_and_add_content (dcp, true);
	BOOST_REQUIRE (!wait_for_jobs ());
	dcp->only_text()->set_use (true);

	Player player(film, Image::Alignment::COMPACT);
	player.set_fast();
	player.set_always_burn_open_subtitles();
	player.set_play_referenced();

	auto butler = std::make_shared<Butler>(
		film, player, AudioMapping(), 2, bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::DISABLED
		);

	for (int i = 0; i < 10; ++i) {
		auto t = DCPTime::from_frames (i, 24);
		butler->seek (t, true);
		auto video = butler->get_video(Butler::Behaviour::BLOCKING, 0);
		BOOST_CHECK_EQUAL(video.second.get(), t.get());
		write_image(video.first->image(bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, true), String::compose("build/test/player_seek_test_%1.png", i));
		/* This 14.08 is empirically chosen (hopefully) to accept changes in rendering between the reference and a test machine
		   (17.10 and 16.04 seem to anti-alias a little differently) but to reject gross errors e.g. missing fonts or missing
		   text altogether.
		*/
		check_image(TestPaths::private_data() / String::compose("player_seek_test_%1.png", i), String::compose("build/test/player_seek_test_%1.png", i), 14.08);
	}
}


/** Test some more seeks towards the start of a DCP with awkward subtitles */
BOOST_AUTO_TEST_CASE (player_seek_test2)
{
	auto film = std::make_shared<Film>(optional<boost::filesystem::path>());
	auto dcp = std::make_shared<DCPContent>(TestPaths::private_data() / "awkward_subs2");
	film->examine_and_add_content (dcp, true);
	BOOST_REQUIRE (!wait_for_jobs ());
	dcp->only_text()->set_use (true);

	Player player(film, Image::Alignment::COMPACT);
	player.set_fast();
	player.set_always_burn_open_subtitles();
	player.set_play_referenced();

	auto butler = std::make_shared<Butler>
		(film, player, AudioMapping(), 2, bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::DISABLED
		 );

	butler->seek(DCPTime::from_seconds(5), true);

	for (int i = 0; i < 10; ++i) {
		auto t = DCPTime::from_seconds(5) + DCPTime::from_frames (i, 24);
		butler->seek (t, true);
		auto video = butler->get_video(Butler::Behaviour::BLOCKING, 0);
		BOOST_CHECK_EQUAL(video.second.get(), t.get());
		write_image(
			video.first->image(bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, true), String::compose("build/test/player_seek_test2_%1.png", i)
			);
		check_image(TestPaths::private_data() / String::compose("player_seek_test2_%1.png", i), String::compose("build/test/player_seek_test2_%1.png", i), 14.08);
	}
}


/** Test a bug when trimmed content follows other content */
BOOST_AUTO_TEST_CASE (player_trim_test)
{
       auto film = new_test_film2 ("player_trim_test");
       auto A = content_factory("test/data/flat_red.png")[0];
       film->examine_and_add_content (A);
       BOOST_REQUIRE (!wait_for_jobs ());
       A->video->set_length (10 * 24);
       auto B = content_factory("test/data/flat_red.png")[0];
       film->examine_and_add_content (B);
       BOOST_REQUIRE (!wait_for_jobs ());
       B->video->set_length (10 * 24);
       B->set_position (film, DCPTime::from_seconds(10));
       B->set_trim_start(film, ContentTime::from_seconds(2));

       make_and_verify_dcp (film);
}


struct Sub {
	PlayerText text;
	TextType type;
	optional<DCPTextTrack> track;
	DCPTimePeriod period;
};


static void
store (list<Sub>* out, PlayerText text, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	Sub s;
	s.text = text;
	s.type = type;
	s.track = track;
	s.period = period;
	out->push_back (s);
}


/** Test ignoring both video and audio */
BOOST_AUTO_TEST_CASE (player_ignore_video_and_audio_test)
{
	auto film = new_test_film2 ("player_ignore_video_and_audio_test");
	auto ff = content_factory(TestPaths::private_data() / "boon_telly.mkv")[0];
	film->examine_and_add_content (ff);
	auto text = content_factory("test/data/subrip.srt")[0];
	film->examine_and_add_content (text);
	BOOST_REQUIRE (!wait_for_jobs());
	text->only_text()->set_type (TextType::CLOSED_CAPTION);
	text->only_text()->set_use (true);

	Player player(film, Image::Alignment::COMPACT);
	player.set_ignore_video();
	player.set_ignore_audio();

	list<Sub> out;
	player.Text.connect(bind (&store, &out, _1, _2, _3, _4));
	while (!player.pass()) {}

	BOOST_CHECK_EQUAL (out.size(), 6U);
}


/** Trigger a crash due to the assertion failure in Player::emit_audio */
BOOST_AUTO_TEST_CASE (player_trim_crash)
{
	auto film = new_test_film2 ("player_trim_crash");
	auto boon = content_factory(TestPaths::private_data() / "boon_telly.mkv")[0];
	film->examine_and_add_content (boon);
	BOOST_REQUIRE (!wait_for_jobs());

	Player player(film, Image::Alignment::COMPACT);
	player.set_fast();
	auto butler = std::make_shared<Butler>(
		film, player, AudioMapping(), 6, bind(&PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::COMPACT, true, false, Butler::Audio::ENABLED
		);

	/* Wait for the butler to fill */
	dcpomatic_sleep_seconds (5);

	boon->set_trim_start(film, ContentTime::from_seconds(5));

	butler->seek (DCPTime(), true);

	/* Wait for the butler to refill */
	dcpomatic_sleep_seconds (5);

	butler->rethrow ();
}


/** Test a crash when the gap between the last audio and the start of a silent period is more than 1 sample */
BOOST_AUTO_TEST_CASE (player_silence_crash)
{
	Cleanup cl;

	auto sine = content_factory("test/data/impulse_train.wav")[0];
	auto film = new_test_film2("player_silence_crash", { sine }, &cl);
	sine->set_video_frame_rate(film, 23.976);
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});

	cl.run();
}


/** Test a crash when processing a 3D DCP */
BOOST_AUTO_TEST_CASE (player_3d_test_1)
{
	auto film = new_test_film2 ("player_3d_test_1a");
	auto left = content_factory("test/data/flat_red.png")[0];
	film->examine_and_add_content (left);
	auto right = content_factory("test/data/flat_blue.png")[0];
	film->examine_and_add_content (right);
	BOOST_REQUIRE (!wait_for_jobs());

	left->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	left->set_position (film, DCPTime());
	right->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	right->set_position (film, DCPTime());
	film->set_three_d (true);

	make_and_verify_dcp (film);

	auto dcp = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2 ("player_3d_test_1b", {dcp});

	film2->set_three_d (true);
	make_and_verify_dcp (film2);
}


/** Test a crash when processing a 3D DCP as content in a 2D project */
BOOST_AUTO_TEST_CASE (player_3d_test_2)
{
	auto left = content_factory("test/data/flat_red.png")[0];
	auto right = content_factory("test/data/flat_blue.png")[0];
	auto film = new_test_film2 ("player_3d_test_2a", {left, right});

	left->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	left->set_position (film, DCPTime());
	right->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	right->set_position (film, DCPTime());
	film->set_three_d (true);

	make_and_verify_dcp (film);

	auto dcp = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2 ("player_3d_test_2b", {dcp});

	make_and_verify_dcp (film2);
}


/** Test a crash when there is video-only content at the end of the DCP and a frame-rate conversion is happening;
 *  #1691.
 */
BOOST_AUTO_TEST_CASE (player_silence_at_end_crash)
{
	/* 25fps DCP with some audio */
	auto content1 = content_factory("test/data/flat_red.png")[0];
	auto film1 = new_test_film2 ("player_silence_at_end_crash_1", {content1});
	content1->video->set_length (25);
	film1->set_video_frame_rate (25);
	make_and_verify_dcp (film1);

	/* Make another project importing this DCP */
	auto content2 = std::make_shared<DCPContent>(film1->dir(film1->dcp_name()));
	auto film2 = new_test_film2 ("player_silence_at_end_crash_2", {content2});

	/* and importing just the video MXF on its own at the end */
	optional<boost::filesystem::path> video;
	for (auto i: boost::filesystem::directory_iterator(film1->dir(film1->dcp_name()))) {
		if (boost::starts_with(i.path().filename().string(), "j2c_")) {
			video = i.path();
		}
	}

	BOOST_REQUIRE (video);
	auto content3 = content_factory(*video)[0];
	film2->examine_and_add_content (content3);
	BOOST_REQUIRE (!wait_for_jobs());
	content3->set_position (film2, DCPTime::from_seconds(1.5));
	film2->set_video_frame_rate (24);
	make_and_verify_dcp (film2);
}


/** #2257 */
BOOST_AUTO_TEST_CASE (encrypted_dcp_with_no_kdm_gives_no_butler_error)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film2 ("encrypted_dcp_with_no_kdm_gives_no_butler_error", { content });
	int constexpr length = 24 * 25;
	content->video->set_length(length);
	film->set_encrypted (true);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});

	auto content2 = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film2 ("encrypted_dcp_with_no_kdm_gives_no_butler_error2", { content2 });

	Player player(film, Image::Alignment::COMPACT);
	Butler butler(film2, player, AudioMapping(), 2, bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::ENABLED);

	float buffer[2000 * 6];
	for (int i = 0; i < length; ++i) {
		butler.get_video(Butler::Behaviour::BLOCKING, 0);
		butler.get_audio(Butler::Behaviour::BLOCKING, buffer, 2000);
	}

	BOOST_CHECK_NO_THROW(butler.rethrow());
}


BOOST_AUTO_TEST_CASE (interleaved_subtitle_are_emitted_correctly)
{
	boost::filesystem::path paths[2] = {
		"build/test/interleaved_subtitle_are_emitted_correctly1.srt",
		"build/test/interleaved_subtitle_are_emitted_correctly2.srt"
	};

	dcp::File subs_file[2] = { dcp::File(paths[0], "w"), dcp::File(paths[1], "w") };

	fprintf(subs_file[0].get(), "1\n00:00:01,000 -> 00:00:02,000\nSub 1/1\n\n");
	fprintf(subs_file[0].get(), "2\n00:00:05,000 -> 00:00:06,000\nSub 1/2\n\n");

	fprintf(subs_file[1].get(), "1\n00:00:00,500 -> 00:00:01,500\nSub 2/1\n\n");
	fprintf(subs_file[1].get(), "2\n00:00:02,000 -> 00:00:03,000\nSub 2/2\n\n");

	subs_file[0].close();
	subs_file[1].close();

	auto subs1 = content_factory(paths[0])[0];
	auto subs2 = content_factory(paths[1])[0];
	auto film = new_test_film2("interleaved_subtitle_are_emitted_correctly", { subs1, subs2 });
	film->set_sequence(false);
	subs1->set_position(film, DCPTime());
	subs2->set_position(film, DCPTime());

	Player player(film, Image::Alignment::COMPACT);
	dcp::Time last;
	player.Text.connect([&last](PlayerText text, TextType, optional<DCPTextTrack>, dcpomatic::DCPTimePeriod) {
		for (auto sub: text.string) {
			BOOST_CHECK(sub.in() >= last);
			last = sub.in();
		}
	});
	while (!player.pass()) {}
}


BOOST_AUTO_TEST_CASE(multiple_sound_files_bug)
{
	Cleanup cl;

	Config::instance()->set_log_types(Config::instance()->log_types() | LogEntry::TYPE_DEBUG_PLAYER);

	auto A = content_factory(TestPaths::private_data() / "kook" / "1.wav").front();
	auto B = content_factory(TestPaths::private_data() / "kook" / "2.wav").front();
	auto C = content_factory(TestPaths::private_data() / "kook" / "3.wav").front();

	auto film = new_test_film2("multiple_sound_files_bug", { A, B, C }, &cl);
	film->set_audio_channels(16);
	C->set_position(film, DCPTime(3840000));

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	check_mxf_audio_file(TestPaths::private_data() / "kook" / "reference.mxf", dcp_file(film, "pcm_"));

	cl.run();
}


BOOST_AUTO_TEST_CASE(trimmed_sound_mix_bug_13)
{
	auto A = content_factory("test/data/sine_16_48_440_10.wav").front();
	auto B = content_factory("test/data/sine_16_44.1_440_10.wav").front();
	auto film = new_test_film2("trimmed_sound_mix_bug_13", { A, B });
	film->set_audio_channels(16);

	A->set_position(film, DCPTime());
	A->audio->set_gain(-12);
	B->set_position(film, DCPTime());
	B->audio->set_gain(-12);
	B->set_trim_start(film, ContentTime(13));

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	check_mxf_audio_file("test/data/trimmed_sound_mix_bug_13.mxf", dcp_file(film, "pcm_"));
}


BOOST_AUTO_TEST_CASE(trimmed_sound_mix_bug_13_frame_rate_change)
{
	auto A = content_factory("test/data/sine_16_48_440_10.wav").front();
	auto B = content_factory("test/data/sine_16_44.1_440_10.wav").front();
	auto film = new_test_film2("trimmed_sound_mix_bug_13_frame_rate_change", { A, B });

	A->set_position(film, DCPTime());
	A->audio->set_gain(-12);
	B->set_position(film, DCPTime());
	B->audio->set_gain(-12);
	B->set_trim_start(film, ContentTime(13));

	A->set_video_frame_rate(film, 24);
	B->set_video_frame_rate(film, 24);
	film->set_video_frame_rate(25);
	film->set_audio_channels(16);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	check_mxf_audio_file("test/data/trimmed_sound_mix_bug_13_frame_rate_change.mxf", dcp_file(film, "pcm_"));
}


BOOST_AUTO_TEST_CASE(two_d_in_three_d_duplicates)
{
	auto A = content_factory("test/data/flat_red.png").front();
	auto B = content_factory("test/data/flat_green.png").front();
	auto film = new_test_film2("two_d_in_three_d_duplicates", { A, B });

	film->set_three_d(true);
	B->video->set_frame_type(VideoFrameType::THREE_D_LEFT_RIGHT);
	B->set_position(film, DCPTime::from_seconds(10));
	B->video->set_custom_size(dcp::Size(1998, 1080));

	auto player = std::make_shared<Player>(film, film->playlist());

	std::vector<uint8_t> red_line(1998 * 3);
	for (int i = 0; i < 1998; ++i) {
		red_line[i * 3] = 255;
	};

	std::vector<uint8_t> green_line(1998 * 3);
	for (int i = 0; i < 1998; ++i) {
		green_line[i * 3 + 1] = 255;
	};

	Eyes last_eyes = Eyes::RIGHT;
	optional<DCPTime> last_time;
	player->Video.connect([&last_eyes, &last_time, &red_line, &green_line](shared_ptr<PlayerVideo> video, dcpomatic::DCPTime time) {
		BOOST_CHECK(last_eyes != video->eyes());
		last_eyes = video->eyes();
		if (video->eyes() == Eyes::LEFT) {
			BOOST_CHECK(!last_time || time == *last_time + DCPTime::from_frames(1, 24));
		} else {
			BOOST_CHECK(time == *last_time);
		}
		last_time = time;

		auto image = video->image([](AVPixelFormat) { return AV_PIX_FMT_RGB24; }, VideoRange::FULL, false);
		auto const size = image->size();
		for (int y = 0; y < size.height; ++y) {
			uint8_t* line = image->data()[0] + y * image->stride()[0];
			if (time < DCPTime::from_seconds(10)) {
				BOOST_REQUIRE_EQUAL(memcmp(line, red_line.data(), 1998 * 3), 0);
			} else {
				BOOST_REQUIRE_EQUAL(memcmp(line, green_line.data(), 1998 * 3), 0);
			}
		}
	});

	BOOST_CHECK(film->length() == DCPTime::from_seconds(20));
	while (!player->pass()) {}
}


BOOST_AUTO_TEST_CASE(three_d_in_two_d_chooses_left)
{
	auto left = content_factory("test/data/flat_red.png").front();
	auto right = content_factory("test/data/flat_green.png").front();
	auto mono = content_factory("test/data/flat_blue.png").front();

	auto film = new_test_film2("three_d_in_two_d_chooses_left", { left, right, mono });

	left->video->set_frame_type(VideoFrameType::THREE_D_LEFT);
	left->set_position(film, dcpomatic::DCPTime());
	right->video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
	right->set_position(film, dcpomatic::DCPTime());

	mono->set_position(film, dcpomatic::DCPTime::from_seconds(10));

	auto player = std::make_shared<Player>(film, film->playlist());

	std::vector<uint8_t> red_line(1998 * 3);
	for (int i = 0; i < 1998; ++i) {
		red_line[i * 3] = 255;
	};

	std::vector<uint8_t> blue_line(1998 * 3);
	for (int i = 0; i < 1998; ++i) {
		blue_line[i * 3 + 2] = 255;
	};

	optional<DCPTime> last_time;
	player->Video.connect([&last_time, &red_line, &blue_line](shared_ptr<PlayerVideo> video, dcpomatic::DCPTime time) {
		BOOST_CHECK(video->eyes() == Eyes::BOTH);
		BOOST_CHECK(!last_time || time == *last_time + DCPTime::from_frames(1, 24));
		last_time = time;

		auto image = video->image([](AVPixelFormat) { return AV_PIX_FMT_RGB24; }, VideoRange::FULL, false);
		auto const size = image->size();
		for (int y = 0; y < size.height; ++y) {
			uint8_t* line = image->data()[0] + y * image->stride()[0];
			if (time < DCPTime::from_seconds(10)) {
				BOOST_REQUIRE_EQUAL(memcmp(line, red_line.data(), 1998 * 3), 0);
			} else {
				BOOST_REQUIRE_EQUAL(memcmp(line, blue_line.data(), 1998 * 3), 0);
			}
		}
	});

	BOOST_CHECK(film->length() == DCPTime::from_seconds(20));
	while (!player->pass()) {}
}


BOOST_AUTO_TEST_CASE(check_seek_with_no_video)
{
	auto content = content_factory(TestPaths::private_data() / "Fight.Club.1999.720p.BRRip.x264-x0r.srt")[0];
	auto film = new_test_film2("check_seek_with_no_video", { content });
	auto player = std::make_shared<Player>(film, film->playlist());

	boost::signals2::signal<void (std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime)> Video;

	optional<dcpomatic::DCPTime> earliest;

	player->Video.connect(
		[&earliest](shared_ptr<PlayerVideo>, dcpomatic::DCPTime time) {
			if (!earliest || time < *earliest) {
				earliest = time;
			}
		});

	player->seek(dcpomatic::DCPTime::from_seconds(60 * 60), false);

	for (int i = 0; i < 10; ++i) {
		player->pass();
	}

	BOOST_REQUIRE(earliest);
	BOOST_CHECK(*earliest >= dcpomatic::DCPTime(60 * 60));
}


BOOST_AUTO_TEST_CASE(unmapped_audio_does_not_raise_buffer_error)
{
	auto content = content_factory(TestPaths::private_data() / "arrietty_JP-EN.mkv")[0];
	auto film = new_test_film2("unmapped_audio_does_not_raise_buffer_error", { content });

	content->audio->set_mapping(AudioMapping(6 * 2, MAX_DCP_AUDIO_CHANNELS));

	Player player(film, Image::Alignment::COMPACT);
	Butler butler(film, player, AudioMapping(), 2, bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::ENABLED);

	/* Wait for the butler thread to run for a while; in the case under test it will throw an exception because
	 * the video buffers are filled but no audio comes.
	 */
	dcpomatic_sleep_seconds(10);
	butler.rethrow();
}


/*
    Copyright (C) 2014-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/audio_buffers.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "lib/image_content.h"
#include "lib/string_text_file_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/text_content.h"
#include "lib/butler.h"
#include "lib/compose.hpp"
#include "lib/cross.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

using std::cout;
using std::list;
using std::pair;
using boost::shared_ptr;
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
	shared_ptr<Film> film = new_test_film ("player_silence_padding_test");
	film->set_name ("player_silence_padding_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	accumulated.reset (new AudioBuffers (film->audio_channels(), 0));

	shared_ptr<Player> player (new Player(film));
	player->Audio.connect (bind (&accumulate, _1, _2));
	while (!player->pass ()) {}
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
	shared_ptr<Film> film = new_test_film ("black_fill_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"));
	film->set_name ("black_fill_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_sequence (false);
	film->set_interop (false);
	shared_ptr<ImageContent> contentA (new ImageContent("test/data/simple_testcard_640x480.png"));
	shared_ptr<ImageContent> contentB (new ImageContent("test/data/simple_testcard_640x480.png"));

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime::from_frames(2, film->video_frame_rate()));
	contentA->video->set_custom_ratio (1.85);
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, film->video_frame_rate()));
	contentB->video->set_custom_ratio (1.85);

	film->make_dcp ();

	BOOST_REQUIRE (!wait_for_jobs());

	boost::filesystem::path ref;
	ref = "test";
	ref /= "data";
	ref /= "black_fill_test";

	boost::filesystem::path check;
	check = "build";
	check /= "test";
	check /= "black_fill_test";
	check /= film->dcp_name();

	check_dcp (ref.string(), check.string());
}

/** Check behaviour with an awkward playlist whose data does not end on a video frame start */
BOOST_AUTO_TEST_CASE (player_subframe_test)
{
	shared_ptr<Film> film = new_test_film ("reels_test7");
	film->set_name ("reels_test7");
	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	shared_ptr<Content> A = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (A);
	BOOST_REQUIRE (!wait_for_jobs ());
	shared_ptr<Content> B = content_factory("test/data/awkward_length.wav").front();
	film->examine_and_add_content (B);
	BOOST_REQUIRE (!wait_for_jobs ());
	film->set_video_frame_rate (24);
	A->video->set_length (3 * 24);

	BOOST_CHECK (A->full_length(film) == DCPTime::from_frames(3 * 24, 24));
	BOOST_CHECK (B->full_length(film) == DCPTime(289920));
	/* Length should be rounded up from B's length to the next video frame */
	BOOST_CHECK (film->length() == DCPTime::from_frames(3 * 24 + 1, 24));

	shared_ptr<Player> player (new Player(film));
	player->setup_pieces ();
	BOOST_REQUIRE_EQUAL (player->_black._periods.size(), 1U);
	BOOST_CHECK (player->_black._periods.front() == DCPTimePeriod(DCPTime::from_frames(3 * 24, 24), DCPTime::from_frames(3 * 24 + 1, 24)));
	BOOST_REQUIRE_EQUAL (player->_silent._periods.size(), 1U);
	BOOST_CHECK (player->_silent._periods.front() == DCPTimePeriod(DCPTime(289920), DCPTime::from_frames(3 * 24 + 1, 24)));
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
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_basic_test_subs");
	film->set_name ("ffmpeg_transcoder_basic_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<StringTextFileContent> s (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<Player> player (new Player(film));
	player->Video.connect (bind (&video, _1, _2));
	player->Audio.connect (bind (&audio, _1, _2));
	video_frames = audio_frames = 0;
	while (!player->pass ()) {
		BOOST_CHECK (abs(video_frames - (audio_frames / 2000)) < 8);
	}
}

/** Test some seeks towards the start of a DCP with awkward subtitles; see mantis #1085
 *  and a number of others.  I thought this was a player seek bug but in fact it was
 *  caused by the subtitle starting just after the start of the video frame and hence
 *  being faded out.
 */
BOOST_AUTO_TEST_CASE (player_seek_test)
{
	shared_ptr<Film> film (new Film (optional<boost::filesystem::path>()));
	shared_ptr<DCPContent> dcp (new DCPContent(TestPaths::private_data() / "awkward_subs"));
	film->examine_and_add_content (dcp, true);
	BOOST_REQUIRE (!wait_for_jobs ());
	dcp->only_text()->set_use (true);

	shared_ptr<Player> player (new Player(film));
	player->set_fast ();
	player->set_always_burn_open_subtitles ();
	player->set_play_referenced ();

	shared_ptr<Butler> butler (new Butler (player, AudioMapping(), 2, bind(PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true));
	butler->disable_audio();

	for (int i = 0; i < 10; ++i) {
		DCPTime t = DCPTime::from_frames (i, 24);
		butler->seek (t, true);
		pair<shared_ptr<PlayerVideo>, DCPTime> video = butler->get_video(true, 0);
		BOOST_CHECK_EQUAL(video.second.get(), t.get());
		write_image(video.first->image(bind(PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true), String::compose("build/test/player_seek_test_%1.png", i));
		/* This 14.08 is empirically chosen (hopefully) to accept changes in rendering between the reference and a test machine
		   (17.10 and 16.04 seem to anti-alias a little differently) but to reject gross errors e.g. missing fonts or missing
		   text altogether.
		*/
		check_image(String::compose("test/data/player_seek_test_%1.png", i), String::compose("build/test/player_seek_test_%1.png", i), 14.08);
	}
}

/** Test some more seeks towards the start of a DCP with awkward subtitles */
BOOST_AUTO_TEST_CASE (player_seek_test2)
{
	shared_ptr<Film> film (new Film (optional<boost::filesystem::path>()));
	shared_ptr<DCPContent> dcp (new DCPContent(TestPaths::private_data() / "awkward_subs2"));
	film->examine_and_add_content (dcp, true);
	BOOST_REQUIRE (!wait_for_jobs ());
	dcp->only_text()->set_use (true);

	shared_ptr<Player> player (new Player(film));
	player->set_fast ();
	player->set_always_burn_open_subtitles ();
	player->set_play_referenced ();

	shared_ptr<Butler> butler (new Butler(player, AudioMapping(), 2, bind(PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true));
	butler->disable_audio();

	butler->seek(DCPTime::from_seconds(5), true);

	for (int i = 0; i < 10; ++i) {
		DCPTime t = DCPTime::from_seconds(5) + DCPTime::from_frames (i, 24);
		butler->seek (t, true);
		pair<shared_ptr<PlayerVideo>, DCPTime> video = butler->get_video(true, 0);
		BOOST_CHECK_EQUAL(video.second.get(), t.get());
		write_image(video.first->image(bind(PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true), String::compose("build/test/player_seek_test2_%1.png", i));
		check_image(String::compose("test/data/player_seek_test2_%1.png", i), String::compose("build/test/player_seek_test2_%1.png", i), 14.08);
	}
}

/** Test a bug when trimmed content follows other content */
BOOST_AUTO_TEST_CASE (player_trim_test)
{
       shared_ptr<Film> film = new_test_film2 ("player_trim_test");
       shared_ptr<Content> A = content_factory("test/data/flat_red.png").front();
       film->examine_and_add_content (A);
       BOOST_REQUIRE (!wait_for_jobs ());
       A->video->set_length (10 * 24);
       shared_ptr<Content> B = content_factory("test/data/flat_red.png").front();
       film->examine_and_add_content (B);
       BOOST_REQUIRE (!wait_for_jobs ());
       B->video->set_length (10 * 24);
       B->set_position (film, DCPTime::from_seconds(10));
       B->set_trim_start (ContentTime::from_seconds (2));

       film->make_dcp ();
       BOOST_REQUIRE (!wait_for_jobs ());
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
	shared_ptr<Film> film = new_test_film2 ("player_ignore_video_and_audio_test");
	shared_ptr<Content> ff = content_factory(TestPaths::private_data() / "boon_telly.mkv").front();
	film->examine_and_add_content (ff);
	shared_ptr<Content> text = content_factory("test/data/subrip.srt").front();
	film->examine_and_add_content (text);
	BOOST_REQUIRE (!wait_for_jobs());
	text->only_text()->set_type (TEXT_CLOSED_CAPTION);
	text->only_text()->set_use (true);

	shared_ptr<Player> player (new Player(film));
	player->set_ignore_video ();
	player->set_ignore_audio ();

	list<Sub> out;
	player->Text.connect (bind (&store, &out, _1, _2, _3, _4));
	while (!player->pass ()) {}

	BOOST_CHECK_EQUAL (out.size(), 6U);
}

/** Trigger a crash due to the assertion failure in Player::emit_audio */
BOOST_AUTO_TEST_CASE (player_trim_crash)
{
	shared_ptr<Film> film = new_test_film2 ("player_trim_crash");
	shared_ptr<Content> boon = content_factory(TestPaths::private_data() / "boon_telly.mkv").front();
	film->examine_and_add_content (boon);
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Player> player (new Player(film));
	player->set_fast ();
	shared_ptr<Butler> butler (new Butler(player, AudioMapping(), 6, bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true));

	/* Wait for the butler to fill */
	dcpomatic_sleep_seconds (5);

	boon->set_trim_start (ContentTime::from_seconds(5));

	butler->seek (DCPTime(), true);

	/* Wait for the butler to refill */
	dcpomatic_sleep_seconds (5);

	butler->rethrow ();
}

/** Test a crash when the gap between the last audio and the start of a silent period is more than 1 sample */
BOOST_AUTO_TEST_CASE (player_silence_crash)
{
	shared_ptr<Film> film = new_test_film2 ("player_silence_crash");
	shared_ptr<Content> sine = content_factory("test/data/impulse_train.wav").front();
	film->examine_and_add_content (sine);
	BOOST_REQUIRE (!wait_for_jobs());

	sine->set_video_frame_rate (23.976);
	film->write_metadata ();
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

/** Test a crash when processing a 3D DCP */
BOOST_AUTO_TEST_CASE (player_3d_test_1)
{
	shared_ptr<Film> film = new_test_film2 ("player_3d_test_1a");
	shared_ptr<Content> left = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (left);
	shared_ptr<Content> right = content_factory("test/data/flat_blue.png").front();
	film->examine_and_add_content (right);
	BOOST_REQUIRE (!wait_for_jobs());

	left->video->set_frame_type (VIDEO_FRAME_TYPE_3D_LEFT);
	left->set_position (film, DCPTime());
	right->video->set_frame_type (VIDEO_FRAME_TYPE_3D_RIGHT);
	right->set_position (film, DCPTime());
	film->set_three_d (true);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Film> film2 = new_test_film2 ("player_3d_test_1b");
	shared_ptr<Content> dcp(new DCPContent(film->dir(film->dcp_name())));
	film2->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());

	film2->set_three_d (true);
	film2->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

/** Test a crash when processing a 3D DCP as content in a 2D project */
BOOST_AUTO_TEST_CASE (player_3d_test_2)
{
	shared_ptr<Film> film = new_test_film2 ("player_3d_test_2a");
	shared_ptr<Content> left = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (left);
	shared_ptr<Content> right = content_factory("test/data/flat_blue.png").front();
	film->examine_and_add_content (right);
	BOOST_REQUIRE (!wait_for_jobs());

	left->video->set_frame_type (VIDEO_FRAME_TYPE_3D_LEFT);
	left->set_position (film, DCPTime());
	right->video->set_frame_type (VIDEO_FRAME_TYPE_3D_RIGHT);
	right->set_position (film, DCPTime());
	film->set_three_d (true);

	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Film> film2 = new_test_film2 ("player_3d_test_2b");
	shared_ptr<Content> dcp(new DCPContent(film->dir(film->dcp_name())));
	film2->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());

	film2->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

/** Test a crash when there is video-only content at the end of the DCP and a frame-rate conversion is happening;
 *  #1691.
 */
BOOST_AUTO_TEST_CASE (player_silence_at_end_crash)
{
	/* 25fps DCP with some audio */
	shared_ptr<Film> film1 = new_test_film2 ("player_silence_at_end_crash_1");
	shared_ptr<Content> content1 = content_factory("test/data/flat_red.png").front();
	film1->examine_and_add_content (content1);
	BOOST_REQUIRE (!wait_for_jobs());
	content1->video->set_length (25);
	film1->set_video_frame_rate (25);
	film1->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make another project importing this DCP */
	shared_ptr<Film> film2 = new_test_film2 ("player_silence_at_end_crash_2");
	shared_ptr<Content> content2(new DCPContent(film1->dir(film1->dcp_name())));
	film2->examine_and_add_content (content2);
	BOOST_REQUIRE (!wait_for_jobs());

	/* and importing just the video MXF on its own at the end */
	optional<boost::filesystem::path> video;
	for (boost::filesystem::directory_iterator i(film1->dir(film1->dcp_name())); i != boost::filesystem::directory_iterator(); ++i) {
		if (boost::starts_with(i->path().filename().string(), "j2c_")) {
			video = i->path();
		}
	}

	BOOST_REQUIRE (video);
	shared_ptr<Content> content3 = content_factory(*video).front();
	film2->examine_and_add_content (content3);
	BOOST_REQUIRE (!wait_for_jobs());
	content3->set_position (film2, DCPTime::from_seconds(1.5));
	film2->set_video_frame_rate (24);
	film2->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());
}

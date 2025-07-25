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


#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_examiner.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/ratio.h"
#include "lib/string_text_file_content.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/file.h>
#include <dcp/raw_convert.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::string;
using std::vector;
using namespace dcpomatic;


static void
ffmpeg_content_test (int number, boost::filesystem::path content, ExportFormat format)
{
	Cleanup cl;

	string name = "ffmpeg_encoder_";
	string extension;
	switch (format) {
	case ExportFormat::H264_AAC:
		name += "h264";
		extension = "mp4";
		break;
	case ExportFormat::PRORES_4444:
		name += "prores-444";
		extension = "mov";
		break;
	case ExportFormat::PRORES_HQ:
		name += "prores-hq";
		extension = "mov";
		break;
	case ExportFormat::PRORES_LT:
		name += "prores-lt";
		extension = "mov";
		break;
	case ExportFormat::SUBTITLES_DCP:
		BOOST_REQUIRE (false);
	}

	name = fmt::format("{}_test{}", name, number);

	auto c = make_shared<FFmpegContent>(content);
	auto film = new_test_film(name, {c}, &cl);
	film->set_name (name);
	film->set_audio_channels (6);

	film->write_metadata ();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto file = boost::filesystem::path("build") / "test" / fmt::format("{}.{}", name, extension);
	cl.add (file);
	FFmpegFilmEncoder encoder(film, job, file, format, false, false, false, 23);
	encoder.go ();

	cl.run ();
}


/** Red / green / blue MP4 -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test1)
{
	ffmpeg_content_test (1, "test/data/test.mp4", ExportFormat::PRORES_HQ);
}


/** Dolby Aurora trailer VOB -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test2)
{
	ffmpeg_content_test (2, TestPaths::private_data() / "dolby_aurora.vob", ExportFormat::PRORES_HQ);
}


/** Sintel trailer -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test3)
{
	ffmpeg_content_test (3, TestPaths::private_data() / "Sintel_Trailer1.480p.DivX_Plus_HD.mkv", ExportFormat::PRORES_HQ);
}


/** Big Buck Bunny trailer -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test4)
{
	ffmpeg_content_test (4, TestPaths::private_data() / "big_buck_bunny_trailer_480p.mov", ExportFormat::PRORES_HQ);
}


/** Still image -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test5)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_prores_test5.mov";
	Cleanup cl;
	cl.add(output);

	auto c = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_prores_test5", { c });
	film->set_audio_channels (6);

	c->video->set_length (240);

	film->write_metadata ();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go ();

	cl.run();
}


/** Subs -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test6)
{
	auto s = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	auto film = new_test_film("ffmpeg_encoder_prores_test6", { s });
	film->set_audio_channels (6);

	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob> (film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_prores_test6.mov", ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go ();
}


/** Video + subs -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test7)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_prores_test7.mov";
	Cleanup cl;
	cl.add(output);

	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	auto s = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("ffmpeg_encoder_prores_test7", { c, s });
	film->set_audio_channels (6);

	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go ();

	cl.run();
}


/** Red / green / blue MP4 -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test1)
{
	ffmpeg_content_test(1, "test/data/test.mp4", ExportFormat::H264_AAC);
}


/** Just subtitles -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test2)
{
	auto s = make_shared<StringTextFileContent>("test/data/subrip2.srt");
	auto film = new_test_film("ffmpeg_encoder_h264_test2", { s });
	film->set_audio_channels (6);

	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test2.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go ();
}


/** Video + subs -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test3)
{
	auto c = make_shared<FFmpegContent>("test/data/test.mp4");
	auto s = make_shared<StringTextFileContent>("test/data/subrip.srt");
	auto film = new_test_film("ffmpeg_encoder_h264_test3", { c, s });
	film->set_audio_channels (6);

	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::Effect::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	auto job = make_shared<TranscodeJob> (film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test3.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go ();
}


/** Scope-in-flat DCP -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test4)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test4", {make_shared<DCPContent>("test/data/scope_dcp")});
	BOOST_REQUIRE(!wait_for_jobs());

	film->set_container(Ratio::from_id("185"));

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test4.mp4", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go();
}


/** Test mixdown from 5.1 to stereo */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test5)
{
	auto L = make_shared<FFmpegContent>("test/data/L.wav");
	auto R = make_shared<FFmpegContent>("test/data/R.wav");
	auto C = make_shared<FFmpegContent>("test/data/C.wav");
	auto Ls = make_shared<FFmpegContent>("test/data/Ls.wav");
	auto Rs = make_shared<FFmpegContent>("test/data/Rs.wav");
	auto Lfe = make_shared<FFmpegContent>("test/data/Lfe.wav");

	auto film = new_test_film("ffmpeg_encoder_h264_test5", { L, R, C, Ls, Rs, Lfe });
	film->set_audio_channels (6);

	AudioMapping map (1, MAX_DCP_AUDIO_CHANNELS);

	L->set_position (film, DCPTime::from_seconds(0));
	map.make_zero ();
	map.set (0, 0, 1);
	L->audio->set_mapping (map);
	R->set_position (film, DCPTime::from_seconds(1));
	map.make_zero ();
	map.set (0, 1, 1);
	R->audio->set_mapping (map);
	C->set_position (film, DCPTime::from_seconds(2));
	map.make_zero ();
	map.set (0, 2, 1);
	C->audio->set_mapping (map);
	Lfe->set_position (film, DCPTime::from_seconds(3));
	map.make_zero ();
	map.set (0, 3, 1);
	Lfe->audio->set_mapping (map);
	Ls->set_position (film, DCPTime::from_seconds(4));
	map.make_zero ();
	map.set (0, 4, 1);
	Ls->audio->set_mapping (map);
	Rs->set_position (film, DCPTime::from_seconds(5));
	map.make_zero ();
	map.set (0, 5, 1);
	Rs->audio->set_mapping (map);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test5.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go ();

	check_ffmpeg ("build/test/ffmpeg_encoder_h264_test5.mp4", "test/data/ffmpeg_encoder_h264_test5.mp4", 1);
}


/** Test export of a VF */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test6)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test6_ov");
	film->examine_and_add_content({make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png")});
	BOOST_REQUIRE (!wait_for_jobs());
	make_and_verify_dcp (film);

	auto film2 = new_test_film("ffmpeg_encoder_h264_test6_vf");
	auto ov = make_shared<DCPContent>("build/test/ffmpeg_encoder_h264_test6_ov/" + film->dcp_name(false));
	film2->examine_and_add_content({ov});
	BOOST_REQUIRE (!wait_for_jobs());
	ov->set_reference_video (true);
	auto subs = content_factory("test/data/subrip.srt")[0];
	film2->examine_and_add_content({subs});
	BOOST_REQUIRE (!wait_for_jobs());
	for (auto i: subs->text) {
		i->set_use (true);
	}

	auto job = make_shared<TranscodeJob>(film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, "build/test/ffmpeg_encoder_h264_test6_vf.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go ();
}


/** Test export of a 3D DCP in a 2D project */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_3d_dcp_to_h264)
{
	boost::filesystem::path const output = "build/test/ffmpeg_encoder_3d_dcp_to_h264.mp4";
	Cleanup cl;
	cl.add(output);

	auto dcp = make_shared<DCPContent>(TestPaths::private_data() / "xm");
	auto film2 = new_test_film("ffmpeg_encoder_3d_dcp_to_h264_export", {dcp});

	auto job = make_shared<TranscodeJob>(film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, output, ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go ();

	cl.run();
}


/** Test export of a 3D DCP in a 2D project */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test7)
{
	auto L = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto R = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_h264_test7_data", {L, R});
	L->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	L->set_position (film, DCPTime());
	R->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	R->set_position (film, DCPTime());
	film->set_three_d (true);
	make_and_verify_dcp (film);

	auto dcp = make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film("ffmpeg_encoder_h264_test7_export", {dcp});

	auto job = make_shared<TranscodeJob> (film2, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film2, job, "build/test/ffmpeg_encoder_h264_test7.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go ();
}


BOOST_AUTO_TEST_CASE(ffmpeg_encoder_2d_content_in_3d_project)
{
	auto content = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_2d_content_in_3d_project", { content });
	film->set_three_d(true);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_2d_content_in_3d_project.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


/** Stereo project with mixdown-to-stereo set */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test8)
{
	auto film = new_test_film("ffmpeg_encoder_h264_test4");
	film->examine_and_add_content({make_shared<DCPContent>("test/data/scope_dcp")});
	BOOST_REQUIRE(!wait_for_jobs());
	film->set_audio_channels (2);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test8.mp4", ExportFormat::H264_AAC, true, false, false, 23);
	encoder.go();
}


/** 7.1/HI/VI (i.e. 12-channel) project */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test9)
{
	auto c = make_shared<ImageContent>(TestPaths::private_data() / "bbc405.png");
	auto film = new_test_film("ffmpeg_encoder_prores_test9", { c });
	film->set_name ("ffmpeg_encoder_prores_test9");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (12);

	film->examine_and_add_content({c});
	BOOST_REQUIRE (!wait_for_jobs ());

	c->video->set_length (240);

	film->write_metadata ();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_test9.mov", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go ();
}


/** DCP -> Prores with crop */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_from_dcp_with_crop)
{
	auto dcp = make_shared<DCPContent>("test/data/import_dcp_test2");
	auto film = new_test_film("ffmpeg_encoder_prores_from_dcp_with_crop", { dcp });
	dcp->video->set_left_crop (32);
	dcp->video->set_right_crop (32);
	film->write_metadata ();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_from_dcp_with_crop.mov", ExportFormat::PRORES_HQ, false, false, false, 23);
	encoder.go ();
}


/** DCP -> H264 with crop */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_from_dcp_with_crop)
{
	auto dcp = make_shared<DCPContent>("test/data/import_dcp_test2");
	auto film = new_test_film("ffmpeg_encoder_h264_from_dcp_with_crop", { dcp });
	dcp->video->set_left_crop (32);
	dcp->video->set_right_crop (32);
	film->write_metadata ();

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_from_dcp_with_crop.mov", ExportFormat::H264_AAC, false, false, false, 23);
	encoder.go ();
}


/** Export to H264 with reels */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_with_reels)
{
	auto content1 = content_factory("test/data/flat_red.png")[0];
	auto content2 = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("ffmpeg_encoder_h264_with_reels", { content1, content2 });
	film->set_reel_type (ReelType::BY_VIDEO_CONTENT);
	content1->video->set_length (240);
	content2->video->set_length (240);

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_h264_with_reels.mov", ExportFormat::H264_AAC, false, true, false, 23);
	encoder.go ();

	auto check = [](boost::filesystem::path path) {
		auto reel = std::dynamic_pointer_cast<FFmpegContent>(content_factory(path)[0]);
		BOOST_REQUIRE (reel);
		FFmpegExaminer examiner(reel);
		BOOST_CHECK_EQUAL (examiner.video_length(), 240U);
	};

	check ("build/test/ffmpeg_encoder_h264_with_reels_reel1.mov");
	check ("build/test/ffmpeg_encoder_h264_with_reels_reel2.mov");
}


/** Regression test for "Error during decoding: Butler finished" (#2097) */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_regression_1)
{
	Cleanup cl;

	auto content = content_factory(TestPaths::private_data() / "arrietty_JP-EN.mkv")[0];
	auto film = new_test_film("ffmpeg_encoder_prores_regression_1", { content });

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_regression_1.mov", ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go ();

	cl.add("build/test/ffmpeg_encoder_prores_regression_1.mov");
	cl.run();
}


/** Regression test for Butler video buffers reached 480 frames (audio is 0) (#2101) */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_regression_2)
{
	Cleanup cl;

	auto logs = dcpomatic_log->types();
	dcpomatic_log->set_types(logs | LogEntry::TYPE_DEBUG_PLAYER);

	auto content = content_factory(TestPaths::private_data() / "tge_clip.mkv")[0];
	auto film = new_test_film("ffmpeg_encoder_prores_regression_2", { content });

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_regression_2.mov", ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go ();

	dcpomatic_log->set_types(logs);

	cl.add("build/test/ffmpeg_encoder_prores_regression_2.mov");
	cl.run();
}


BOOST_AUTO_TEST_CASE(ffmpeg_encoder_missing_frame_at_end)
{
	auto content = content_factory(TestPaths::private_data() / "1s1f.mov");
	auto film = new_test_film("ffmpeg_encoder_missing_frame_at_end", content);

	boost::filesystem::path output("build/test/ffmpeg_encoder_missing_frame_at_end.mov");
	boost::filesystem::path log("build/test/ffmpeg_encoder_missing_frame_at_end.log");

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	FFmpegFilmEncoder encoder(film, job, output, ExportFormat::PRORES_HQ, false, true, false, 23);
	encoder.go();

	run_ffprobe(output, log, false, "-show_frames -show_format -show_streams -select_streams v:0");

	dcp::File read_log(log, "r");
	BOOST_REQUIRE(read_log);

	int nb_read_frames = 0;
	int nb_frames = 0;
	char buffer[256];

	while (!read_log.eof()) {
		read_log.gets(buffer, sizeof(buffer));
		vector<string> parts;
		boost::algorithm::split(parts, buffer, boost::is_any_of("="));
		if (parts.size() == 2) {
			if (parts[0] == "nb_read_frames") {
				nb_read_frames = dcp::raw_convert<int>(parts[1]);
			} else if (parts[0] == "nb_frames") {
				nb_frames = dcp::raw_convert<int>(parts[1]);
			}
		}
	}

	BOOST_CHECK_EQUAL(nb_frames, 26);
	BOOST_CHECK_EQUAL(nb_read_frames, 26);
}


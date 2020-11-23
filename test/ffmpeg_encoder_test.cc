/*
    Copyright (C) 2017-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/ffmpeg_encoder.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/image_content.h"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include "lib/string_text_file_content.h"
#include "lib/ratio.h"
#include "lib/transcode_job.h"
#include "lib/dcp_content.h"
#include "lib/text_content.h"
#include "lib/compose.hpp"
#include "lib/content_factory.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::string;
using boost::shared_ptr;
using boost::optional;
using namespace dcpomatic;

static void
ffmpeg_content_test (int number, boost::filesystem::path content, ExportFormat format)
{
	string name = "ffmpeg_encoder_";
	string extension;
	switch (format) {
	case EXPORT_FORMAT_H264_AAC:
		name += "h264";
		extension = "mp4";
		break;
	case EXPORT_FORMAT_PRORES:
		name += "prores";
		extension = "mov";
		break;
	case EXPORT_FORMAT_H264_PCM:
	case EXPORT_FORMAT_SUBTITLES_DCP:
		BOOST_REQUIRE (false);
	}

	name = String::compose("%1_test%2", name, number);

	shared_ptr<Film> film = new_test_film (name);
	film->set_name (name);
	shared_ptr<FFmpegContent> c (new FFmpegContent(content));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	film->write_metadata ();
	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, String::compose("build/test/%1.%2", name, extension), format, false, false, false, 23);
	encoder.go ();
}

/** Red / green / blue MP4 -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test1)
{
	ffmpeg_content_test (1, "test/data/test.mp4", EXPORT_FORMAT_PRORES);
}

/** Dolby Aurora trailer VOB -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test2)
{
	ffmpeg_content_test (2, TestPaths::private_data() / "dolby_aurora.vob", EXPORT_FORMAT_PRORES);
}

/** Sintel trailer -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test3)
{
	ffmpeg_content_test (3, TestPaths::private_data() / "Sintel_Trailer1.480p.DivX_Plus_HD.mkv", EXPORT_FORMAT_PRORES);
}

/** Big Buck Bunny trailer -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test4)
{
	ffmpeg_content_test (4, TestPaths::private_data() / "big_buck_bunny_trailer_480p.mov", EXPORT_FORMAT_PRORES);
}

/** Still image -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test5)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_prores_test5");
	film->set_name ("ffmpeg_encoder_prores_test5");
	shared_ptr<ImageContent> c (new ImageContent(TestPaths::private_data() / "bbc405.png"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	c->video->set_length (240);

	film->write_metadata ();
	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_test5.mov", EXPORT_FORMAT_PRORES, false, false, false, 23);
	encoder.go ();
}

/** Subs -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test6)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_prores_test6");
	film->set_name ("ffmpeg_encoder_prores_test6");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<StringTextFileContent> s (new StringTextFileContent("test/data/subrip2.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_test6.mov", EXPORT_FORMAT_PRORES, false, false, false, 23);
	encoder.go ();
}

/** Video + subs -> Prores */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_prores_test7)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_prores_test7");
	film->set_name ("ffmpeg_encoder_prores_test7");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<StringTextFileContent> s (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_test7.mov", EXPORT_FORMAT_PRORES, false, false, false, 23);
	encoder.go ();
}

/** Red / green / blue MP4 -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test1)
{
	ffmpeg_content_test(1, "test/data/test.mp4", EXPORT_FORMAT_H264_AAC);
}

/** Just subtitles -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test2)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_h264_test2");
	film->set_name ("ffmpeg_encoder_h264_test2");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<StringTextFileContent> s (new StringTextFileContent("test/data/subrip2.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_h264_test2.mp4", EXPORT_FORMAT_H264_AAC, false, false, false, 23);
	encoder.go ();
}

/** Video + subs -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test3)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_h264_test3");
	film->set_name ("ffmpeg_encoder_h264_test3");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> c (new FFmpegContent("test/data/test.mp4"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<StringTextFileContent> s (new StringTextFileContent("test/data/subrip.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->only_text()->set_colour (dcp::Colour (255, 255, 0));
	s->only_text()->set_effect (dcp::SHADOW);
	s->only_text()->set_effect_colour (dcp::Colour (0, 255, 255));
	film->write_metadata();

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_h264_test3.mp4", EXPORT_FORMAT_H264_AAC, false, false, false, 23);
	encoder.go ();
}

/** Scope-in-flat DCP -> H264 */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test4)
{
	shared_ptr<Film> film = new_test_film2("ffmpeg_encoder_h264_test4");
	film->examine_and_add_content(shared_ptr<DCPContent>(new DCPContent("test/data/scope_dcp")));
	BOOST_REQUIRE(!wait_for_jobs());

	film->set_container(Ratio::from_id("185"));

	shared_ptr<Job> job(new TranscodeJob(film));
	FFmpegEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test4.mp4", EXPORT_FORMAT_H264_AAC, false, false, false, 23);
	encoder.go();
}

/** Test mixdown from 5.1 to stereo */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test5)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_h264_test5");
	film->set_name ("ffmpeg_transcoder_h264_test5");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> L (new FFmpegContent("test/data/L.wav"));
	film->examine_and_add_content (L);
	shared_ptr<FFmpegContent> R (new FFmpegContent("test/data/R.wav"));
	film->examine_and_add_content (R);
	shared_ptr<FFmpegContent> C (new FFmpegContent("test/data/C.wav"));
	film->examine_and_add_content (C);
	shared_ptr<FFmpegContent> Ls (new FFmpegContent("test/data/Ls.wav"));
	film->examine_and_add_content (Ls);
	shared_ptr<FFmpegContent> Rs (new FFmpegContent("test/data/Rs.wav"));
	film->examine_and_add_content (Rs);
	shared_ptr<FFmpegContent> Lfe (new FFmpegContent("test/data/Lfe.wav"));
	film->examine_and_add_content (Lfe);
	BOOST_REQUIRE (!wait_for_jobs ());

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

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_h264_test5.mp4", EXPORT_FORMAT_H264_AAC, true, false, false, 23);
	encoder.go ();

	check_ffmpeg ("build/test/ffmpeg_encoder_h264_test5.mp4", "test/data/ffmpeg_encoder_h264_test5.mp4", 1);
}

/** Test export of a VF */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test6)
{
	shared_ptr<Film> film = new_test_film2 ("ffmpeg_encoder_h264_test6_ov");
	film->examine_and_add_content (shared_ptr<ImageContent>(new ImageContent(TestPaths::private_data() / "bbc405.png")));
	BOOST_REQUIRE (!wait_for_jobs());
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Film> film2 = new_test_film2 ("ffmpeg_encoder_h264_test6_vf");
	shared_ptr<DCPContent> ov (new DCPContent("build/test/ffmpeg_encoder_h264_test6_ov/" + film->dcp_name(false)));
	film2->examine_and_add_content (ov);
	BOOST_REQUIRE (!wait_for_jobs());
	ov->set_reference_video (true);
	shared_ptr<Content> subs = content_factory("test/data/subrip.srt").front();
	film2->examine_and_add_content (subs);
	BOOST_REQUIRE (!wait_for_jobs());
	BOOST_FOREACH (shared_ptr<TextContent> i, subs->text) {
		i->set_use (true);
	}

	shared_ptr<Job> job (new TranscodeJob (film2));
	FFmpegEncoder encoder (film2, job, "build/test/ffmpeg_encoder_h264_test6_vf.mp4", EXPORT_FORMAT_H264_AAC, true, false, false, 23);
	encoder.go ();
}

/** Test export of a 3D DCP in a 2D project */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test7)
{
	shared_ptr<Film> film = new_test_film2 ("ffmpeg_encoder_h264_test7_data");
	shared_ptr<Content> L (shared_ptr<ImageContent>(new ImageContent(TestPaths::private_data() / "bbc405.png")));
	film->examine_and_add_content (L);
	shared_ptr<Content> R (shared_ptr<ImageContent>(new ImageContent(TestPaths::private_data() / "bbc405.png")));
	film->examine_and_add_content (R);
	BOOST_REQUIRE (!wait_for_jobs());
	L->video->set_frame_type (VIDEO_FRAME_TYPE_3D_LEFT);
	L->set_position (film, DCPTime());
	R->video->set_frame_type (VIDEO_FRAME_TYPE_3D_RIGHT);
	R->set_position (film, DCPTime());
	film->set_three_d (true);
	film->make_dcp ();
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Film> film2 = new_test_film2 ("ffmpeg_encoder_h264_test7_export");
	shared_ptr<Content> dcp (new DCPContent(film->dir(film->dcp_name())));
	film2->examine_and_add_content (dcp);
	BOOST_REQUIRE (!wait_for_jobs());

	shared_ptr<Job> job (new TranscodeJob (film2));
	FFmpegEncoder encoder (film2, job, "build/test/ffmpeg_encoder_h264_test7.mp4", EXPORT_FORMAT_H264_AAC, true, false, false, 23);
	encoder.go ();
}


/** Stereo project with mixdown-to-stereo set */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test8)
{
	shared_ptr<Film> film = new_test_film2("ffmpeg_encoder_h264_test4");
	film->examine_and_add_content(shared_ptr<DCPContent>(new DCPContent("test/data/scope_dcp")));
	BOOST_REQUIRE(!wait_for_jobs());
	film->set_audio_channels (2);

	shared_ptr<Job> job(new TranscodeJob(film));
	FFmpegEncoder encoder(film, job, "build/test/ffmpeg_encoder_h264_test8.mp4", EXPORT_FORMAT_H264_AAC, true, false, false, 23);
	encoder.go();
}


/** 7.1/HI/VI (i.e. 12-channel) project */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_h264_test9)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_prores_test9");
	film->set_name ("ffmpeg_encoder_prores_test9");
	shared_ptr<ImageContent> c (new ImageContent(TestPaths::private_data() / "bbc405.png"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (12);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	c->video->set_length (240);

	film->write_metadata ();
	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_prores_test9.mov", EXPORT_FORMAT_H264_AAC, false, false, false, 23);
	encoder.go ();
}

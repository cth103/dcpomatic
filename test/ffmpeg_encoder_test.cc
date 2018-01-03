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

#include "lib/ffmpeg_encoder.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/audio_content.h"
#include "lib/text_subtitle_content.h"
#include "lib/ratio.h"
#include "lib/transcode_job.h"
#include "lib/dcp_content.h"
#include "lib/subtitle_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_encoder_basic_test_mov)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_basic_test_mov");
	film->set_name ("ffmpeg_transcoder_basic_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_basic_test.mov", FFmpegEncoder::FORMAT_PRORES, false);
	encoder.go ();
}

BOOST_AUTO_TEST_CASE (ffmpeg_encoder_basic_test_mp4)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_basic_test_mp4");
	film->set_name ("ffmpeg_transcoder_basic_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_basic_test.mp4", FFmpegEncoder::FORMAT_H264, false);
	encoder.go ();
}

/** Simplest possible export subtitle case: just the subtitles */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_test_subs_h264_1)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_test_subs_h264_1");
	film->set_name ("ffmpeg_encoder_test_subs_h264_1");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<TextSubtitleContent> s (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->subtitle->set_colour (dcp::Colour (255, 255, 0));
	s->subtitle->set_shadow (true);
	s->subtitle->set_effect_colour (dcp::Colour (0, 255, 255));

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_test_subs_h264_1.mp4", FFmpegEncoder::FORMAT_H264, false);
	encoder.go ();
}

/** Slightly more complicated example with longer subs and a video to overlay */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_test_subs_h264_2)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_test_subs_h264_2");
	film->set_name ("ffmpeg_encoder_test_subs_h264_2");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<TextSubtitleContent> s (new TextSubtitleContent (film, "test/data/subrip.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->subtitle->set_colour (dcp::Colour (255, 255, 0));
	s->subtitle->set_shadow (true);
	s->subtitle->set_effect_colour (dcp::Colour (0, 255, 255));

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_test_subs_h264_2.mp4", FFmpegEncoder::FORMAT_H264, false);
	encoder.go ();
}

/** Simplest possible export subtitle case: just the subtitles */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_test_subs_prores_1)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_test_subs_prores_1");
	film->set_name ("ffmpeg_encoder_test_subs_prores_1");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<TextSubtitleContent> s (new TextSubtitleContent (film, "test/data/subrip2.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->subtitle->set_colour (dcp::Colour (255, 255, 0));
	s->subtitle->set_shadow (true);
	s->subtitle->set_effect_colour (dcp::Colour (0, 255, 255));

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_test_subs_prores_1.mov", FFmpegEncoder::FORMAT_PRORES, false);
	encoder.go ();
}

/** Slightly more complicated example with longer subs and a video to overlay */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_test_subs_prores_2)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_encoder_test_subs_prores_2");
	film->set_name ("ffmpeg_encoder_test_subs_prores_2");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs ());

	shared_ptr<TextSubtitleContent> s (new TextSubtitleContent (film, "test/data/subrip.srt"));
	film->examine_and_add_content (s);
	BOOST_REQUIRE (!wait_for_jobs ());
	s->subtitle->set_colour (dcp::Colour (255, 255, 0));
	s->subtitle->set_shadow (true);
	s->subtitle->set_effect_colour (dcp::Colour (0, 255, 255));

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_test_subs_prores_2.mov", FFmpegEncoder::FORMAT_PRORES, false);
	encoder.go ();
}

/** Test a bug with export of scope-in-flat DCP content */
BOOST_AUTO_TEST_CASE (ffmpeg_encoder_bug_test_scope)
{
	shared_ptr<Film> film = new_test_film2("ffmpeg_encoder_bug_test_scope");
	film->examine_and_add_content(shared_ptr<DCPContent>(new DCPContent(film, "test/data/scope_dcp")));
	BOOST_REQUIRE(!wait_for_jobs());

	film->set_container(Ratio::from_id("185"));

	shared_ptr<Job> job(new TranscodeJob(film));
	FFmpegEncoder encoder(film, job, "build/test/ffmpeg_encoder_bug_test_scope.mp4", FFmpegEncoder::FORMAT_H264, false);
	encoder.go();
}

BOOST_AUTO_TEST_CASE (ffmpeg_encoder_basic_test_mixdown)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_basic_test_mixdown");
	film->set_name ("ffmpeg_transcoder_basic_test");
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	shared_ptr<FFmpegContent> L (new FFmpegContent (film, "test/data/L.wav"));
	film->examine_and_add_content (L);
	shared_ptr<FFmpegContent> R (new FFmpegContent (film, "test/data/R.wav"));
	film->examine_and_add_content (R);
	shared_ptr<FFmpegContent> C (new FFmpegContent (film, "test/data/C.wav"));
	film->examine_and_add_content (C);
	shared_ptr<FFmpegContent> Ls (new FFmpegContent (film, "test/data/Ls.wav"));
	film->examine_and_add_content (Ls);
	shared_ptr<FFmpegContent> Rs (new FFmpegContent (film, "test/data/Rs.wav"));
	film->examine_and_add_content (Rs);
	shared_ptr<FFmpegContent> Lfe (new FFmpegContent (film, "test/data/Lfe.wav"));
	film->examine_and_add_content (Lfe);
	BOOST_REQUIRE (!wait_for_jobs ());

	AudioMapping map (1, MAX_DCP_AUDIO_CHANNELS);

	L->set_position (DCPTime::from_seconds (0));
	map.make_zero ();
	map.set (0, 0, 1);
	L->audio->set_mapping (map);
	R->set_position (DCPTime::from_seconds (1));
	map.make_zero ();
	map.set (0, 1, 1);
	R->audio->set_mapping (map);
	C->set_position (DCPTime::from_seconds (2));
	map.make_zero ();
	map.set (0, 2, 1);
	C->audio->set_mapping (map);
	Lfe->set_position (DCPTime::from_seconds (3));
	map.make_zero ();
	map.set (0, 3, 1);
	Lfe->audio->set_mapping (map);
	Ls->set_position (DCPTime::from_seconds (4));
	map.make_zero ();
	map.set (0, 4, 1);
	Ls->audio->set_mapping (map);
	Rs->set_position (DCPTime::from_seconds (5));
	map.make_zero ();
	map.set (0, 5, 1);
	Rs->audio->set_mapping (map);

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegEncoder encoder (film, job, "build/test/ffmpeg_encoder_basic_test_mixdown.mp4", FFmpegEncoder::FORMAT_H264, true);
	encoder.go ();

	/* Skip the first video packet when checking as it contains x264 options which can vary between machines
	   (e.g. number of threads used for encoding).
	*/
	check_ffmpeg ("build/test/ffmpeg_encoder_basic_test_mixdown.mp4", "test/data/ffmpeg_encoder_basic_test_mixdown.mp4");
}

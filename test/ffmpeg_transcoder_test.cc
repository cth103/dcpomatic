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

#include "lib/ffmpeg_transcoder.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/ratio.h"
#include "lib/transcode_job.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_transcoder_basic_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_transcoder_basic_test");
	film->set_name ("ffmpeg_transcoder_basic_test");
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/test.mp4"));
	film->set_container (Ratio::from_id ("185"));
	film->set_audio_channels (6);

	film->examine_and_add_content (c);
	wait_for_jobs ();

	shared_ptr<Job> job (new TranscodeJob (film));
	FFmpegTranscoder transcoder (film, job, "build/test/ffmpeg_transcoder_basic_test/test.mov", FFmpegTranscoder::FORMAT_PRORES);
	transcoder.go ();
}

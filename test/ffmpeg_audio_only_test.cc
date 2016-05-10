/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcp_content_type.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;

/** Test the FFmpeg code with audio-only content */
BOOST_AUTO_TEST_CASE (ffmpeg_audio_only_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_audio_only_test");
	film->set_name ("test_film");
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	shared_ptr<FFmpegContent> c (new FFmpegContent (film, "test/data/sine_440.mp3"));
	film->examine_and_add_content (c);
	wait_for_jobs ();
	film->make_dcp ();
	wait_for_jobs ();
}

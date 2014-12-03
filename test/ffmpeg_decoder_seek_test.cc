/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_decoder_seek_test.cc
 *  @brief Check that get_video() returns the frame indexes that we ask for
 *  for FFmpegDecoder.
 *
 *  This doesn't check that the contents of those frames are right, which
 *  it probably should.
 */

#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/log.h"
#include "lib/film.h"
#include "test.h"

using std::cerr;
using std::vector;
using std::list;
using boost::shared_ptr;
using boost::optional;

static void
check (FFmpegDecoder& decoder, int frame)
{
	list<ContentVideo> v;
	v = decoder.get_video (frame, true);
	BOOST_CHECK (v.size() == 1);
	BOOST_CHECK_EQUAL (v.front().frame, frame);
}

static void
test (boost::filesystem::path file, vector<int> frames)
{
	boost::filesystem::path path = private_data / file;
	if (!boost::filesystem::exists (path)) {
		cerr << "Skipping test: " << path.string() << " not found.\n";
		return;
	}

	shared_ptr<Film> film = new_test_film ("ffmpeg_decoder_seek_test_" + file.string());
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, path)); 
	film->examine_and_add_content (content, true);
	wait_for_jobs ();
	shared_ptr<Log> log (new NullLog);
	FFmpegDecoder decoder (content, log);

	for (vector<int>::const_iterator i = frames.begin(); i != frames.end(); ++i) {
		check (decoder, *i);
	}
}

BOOST_AUTO_TEST_CASE (ffmpeg_decoder_seek_test)
{
	vector<int> frames;
	
	frames.clear ();
	frames.push_back (0);
	frames.push_back (42);
	frames.push_back (999);
	frames.push_back (0);

	test ("boon_telly.mkv", frames);
	test ("Sintel_Trailer1.480p.DivX_Plus_HD.mkv", frames);
	
	frames.clear ();
	frames.push_back (15);
	frames.push_back (42);
	frames.push_back (999);
	frames.push_back (15);
	
	test ("prophet_clip.mkv", frames);
}


/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/ffmpeg_examiner_test.cc
 *  @brief Check that the FFmpegExaminer can extract the first video and audio time
 *  correctly from data/count300bd24.m2ts.
 */

#include <boost/test/unit_test.hpp>
#include "lib/ffmpeg_examiner.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_audio_stream.h"
#include "test.h"

using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (ffmpeg_examiner_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_examiner_test");
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/count300bd24.m2ts"));
	shared_ptr<FFmpegExaminer> examiner (new FFmpegExaminer (content));

	BOOST_CHECK_EQUAL (examiner->first_video().get().get(), ContentTime::from_seconds(600).get());
	BOOST_CHECK_EQUAL (examiner->audio_streams().size(), 1U);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[0]->first_audio.get().get(), ContentTime::from_seconds(600).get());
}

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_examiner.h"

BOOST_AUTO_TEST_CASE (ffmpeg_examiner_test)
{
	shared_ptr<Film> film = new_test_film ("ffmpeg_examiner_test");
	shared_ptr<FFmpegContent> content (new FFmpegContent (film, "test/data/count300bd24.m2ts"));
	shared_ptr<FFmpegExaminer> examiner (new FFmpegExaminer (content));

	BOOST_CHECK_EQUAL (examiner->first_video().get(), 57604000);
	BOOST_CHECK_EQUAL (examiner->audio_streams().size(), 1);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[0]->start.get(), 57600000);
}

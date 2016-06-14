/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "lib/video_frame.h"
#include "lib/exceptions.h"
#include <boost/test/unit_test.hpp>

/** Test VideoFrame */
BOOST_AUTO_TEST_CASE (video_frame_test)
{
	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_BOTH) > VideoFrame (0, EYES_BOTH), false);
	BOOST_CHECK_EQUAL (VideoFrame (1, EYES_BOTH) > VideoFrame (0, EYES_BOTH), true);
	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_BOTH) > VideoFrame (1, EYES_BOTH), false);

	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_LEFT) > VideoFrame (0, EYES_LEFT), false);
	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_LEFT) > VideoFrame (0, EYES_RIGHT), false);
	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_RIGHT) > VideoFrame (0, EYES_LEFT), true);
	BOOST_CHECK_EQUAL (VideoFrame (0, EYES_RIGHT) > VideoFrame (1, EYES_LEFT), false);
	BOOST_CHECK_EQUAL (VideoFrame (1, EYES_LEFT) > VideoFrame (0, EYES_RIGHT), true);

	BOOST_CHECK_THROW (VideoFrame (0, EYES_LEFT) > VideoFrame (0, EYES_BOTH), ProgrammingError);
	BOOST_CHECK_THROW (VideoFrame (0, EYES_BOTH) > VideoFrame (0, EYES_RIGHT), ProgrammingError);
}

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

#include <boost/test/unit_test.hpp>
#include "lib/image_decoder.h"
#include "lib/image_content.h"
#include "test.h"

using std::list;
using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (video_decoder_fill_test1)
{
	shared_ptr<Film> film = new_test_film ("video_decoder_fill_test");
	shared_ptr<ImageContent> c (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	ImageDecoder decoder (c);

	decoder.fill_2d (0, 4);
	BOOST_CHECK_EQUAL (decoder._decoded_video.size(), 4);
	list<ContentVideo>::iterator i = decoder._decoded_video.begin();	
	for (int j = 0; j < 4; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j);
		++i;
	}

	decoder.fill_2d (0, 7);
	BOOST_CHECK_EQUAL (decoder._decoded_video.size(), 7);
	i = decoder._decoded_video.begin();	
	for (int j = 0; j < 7; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j);
		++i;
	}
}

BOOST_AUTO_TEST_CASE (video_decoder_fill_test2)
{
	shared_ptr<Film> film = new_test_film ("video_decoder_fill_test");
	shared_ptr<ImageContent> c (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	ImageDecoder decoder (c);

	decoder.fill_3d (0, 4, EYES_LEFT);
	BOOST_CHECK_EQUAL (decoder._decoded_video.size(), 8);
	list<ContentVideo>::iterator i = decoder._decoded_video.begin();	
	for (int j = 0; j < 8; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j / 2);
		BOOST_CHECK_EQUAL (i->eyes, (j % 2) == 0 ? EYES_LEFT : EYES_RIGHT);
		++i;
	}

	decoder.fill_3d (0, 7, EYES_RIGHT);
	BOOST_CHECK_EQUAL (decoder._decoded_video.size(), 15);
	i = decoder._decoded_video.begin();	
	for (int j = 0; j < 15; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j / 2);
		BOOST_CHECK_EQUAL (i->eyes, (j % 2) == 0 ? EYES_LEFT : EYES_RIGHT);
		++i;
	}
}

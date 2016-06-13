/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include <boost/test/unit_test.hpp>
#include "lib/image_decoder.h"
#include "lib/image_content.h"
#include "lib/content_video.h"
#include "lib/video_decoder.h"
#include "lib/film.h"
#include "test.h"
#include <iostream>

using std::list;
using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (video_decoder_fill_test1)
{
	shared_ptr<Film> film = new_test_film ("video_decoder_fill_test");
	shared_ptr<ImageContent> c (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	ImageDecoder decoder (c, film->log());

	decoder.video->fill_one_eye (0, 4, EYES_BOTH);
	BOOST_CHECK_EQUAL (decoder.video->_decoded.size(), 4U);
	list<ContentVideo>::iterator i = decoder.video->_decoded.begin();
	for (int j = 0; j < 4; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j);
		++i;
	}

	decoder.video->_decoded.clear ();

	decoder.video->fill_one_eye (0, 7, EYES_BOTH);
	BOOST_CHECK_EQUAL (decoder.video->_decoded.size(), 7);
	i = decoder.video->_decoded.begin();
	for (int j = 0; j < 7; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j);
		++i;
	}
}

BOOST_AUTO_TEST_CASE (video_decoder_fill_test2)
{
	shared_ptr<Film> film = new_test_film ("video_decoder_fill_test");
	shared_ptr<ImageContent> c (new ImageContent (film, "test/data/simple_testcard_640x480.png"));
	ImageDecoder decoder (c, film->log());

	decoder.video->fill_both_eyes (0, EYES_LEFT, 4, EYES_LEFT);
	BOOST_CHECK_EQUAL (decoder.video->_decoded.size(), 8);
	list<ContentVideo>::iterator i = decoder.video->_decoded.begin();
	for (int j = 0; j < 8; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j / 2);
		BOOST_CHECK_EQUAL (i->eyes, (j % 2) == 0 ? EYES_LEFT : EYES_RIGHT);
		++i;
	}

	decoder.video->_decoded.clear ();
	decoder.video->fill_both_eyes (0, EYES_LEFT, 7, EYES_RIGHT);
	BOOST_CHECK_EQUAL (decoder.video->_decoded.size(), 15);
	i = decoder.video->_decoded.begin();
	for (int j = 0; j < 15; ++j) {
		BOOST_CHECK_EQUAL (i->frame, j / 2);
		BOOST_CHECK_EQUAL (i->eyes, (j % 2) == 0 ? EYES_LEFT : EYES_RIGHT);
		++i;
	}

	decoder.video->_decoded.clear ();
	decoder.video->fill_both_eyes (0, EYES_RIGHT, 7, EYES_RIGHT);
	BOOST_CHECK_EQUAL (decoder.video->_decoded.size(), 14);
	i = decoder.video->_decoded.begin();
	for (int j = 0; j < 14; ++j) {
		BOOST_CHECK_EQUAL (i->frame, (j + 1) / 2);
		BOOST_CHECK_EQUAL (i->eyes, (j % 2) == 0 ? EYES_RIGHT : EYES_LEFT);
		++i;
	}
}

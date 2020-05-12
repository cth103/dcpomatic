/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "lib/ratio.h"
#include "lib/video_content.h"
#include <boost/test/unit_test.hpp>


static dcp::Size const FOUR_TO_THREE(1436, 1080);
static dcp::Size const FLAT(1998, 1080);
static dcp::Size const SCOPE(2048, 858);


/* Test VideoContent::scaled_size() without any legacy stuff */
BOOST_AUTO_TEST_CASE (scaled_size_test1)
{
	VideoContent vc (0);

	/* Images at full size and in DCP-approved sizes that will not be scaled */
	// Flat/scope content into flat/scope container
	vc._size = FLAT;
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FLAT);
	vc._size = SCOPE;
	BOOST_CHECK_EQUAL (vc.scaled_size(SCOPE), SCOPE);
	// 1.33:1 into flat container
	vc._size = FOUR_TO_THREE;
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(FOUR_TO_THREE));
	// Scope into flat container
	vc._size = SCOPE;
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 837));

	/* Smaller images but in the same ratios */
	vc._size = dcp::Size(185, 100);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FLAT);
	vc._size = dcp::Size(955, 400);
	BOOST_CHECK_EQUAL (vc.scaled_size(SCOPE), SCOPE);
	// 1.33:1 into flat container
	vc._size = dcp::Size(133, 100);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(FOUR_TO_THREE));
	// Scope into flat container
	vc._size = dcp::Size(239, 100);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 836));

	/* Images at full size that are not DCP-approved but will still remain unscaled */
	vc._size = dcp::Size(600, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(600, 1080));
	vc._size = dcp::Size(1700, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1700, 1080));

	/* Image at full size that is too big for the container and will be shrunk */
	vc._size = dcp::Size(3000, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 719));
}


/* Same as scaled_size_test1 but with a non-unity sample aspect ratio */
BOOST_AUTO_TEST_CASE (scaled_size_test2)
{
	VideoContent vc (0);

	vc._sample_aspect_ratio = 2;

	/* Images at full size and in DCP-approved sizes that will not be scaled */
	// Flat/scope content into flat/scope container
	vc._size = dcp::Size (1998 / 2, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FLAT);
	vc._size = dcp::Size (2048 / 2, 858);
	BOOST_CHECK_EQUAL (vc.scaled_size(SCOPE), SCOPE);
	// 1.33:1 into flat container
	vc._size = dcp::Size (1436 / 2, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(FOUR_TO_THREE));
	// Scope into flat container
	vc._size = dcp::Size (2048 / 2, 858);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 837));

	/* Smaller images but in the same ratios */
	vc._size = dcp::Size(185, 200);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FLAT);
	vc._size = dcp::Size(955, 800);
	BOOST_CHECK_EQUAL (vc.scaled_size(SCOPE), SCOPE);
	// 4:3 into flat container
	vc._size = dcp::Size(133, 200);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(FOUR_TO_THREE));
	// Scope into flat container
	vc._size = dcp::Size(239, 200);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 836));

	/* Images at full size that are not DCP-approved but will still remain unscaled */
	vc._size = dcp::Size(600 / 2, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(600, 1080));
	vc._size = dcp::Size(1700 / 2, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1700, 1080));

	/* Image at full size that is too big for the container and will be shrunk */
	vc._size = dcp::Size(3000 / 2, 1080);
	BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(1998, 719));
}


/* Test VideoContent::scaled_size() with some legacy stuff */
BOOST_AUTO_TEST_CASE (scaled_size_legacy_test)
{
	{
		/* 640x480 content that the user had asked to be stretched to 1.85:1 */
		VideoContent vc (0);
		vc._size = dcp::Size(640, 480);
		vc._legacy_ratio = Ratio::from_id("185")->ratio();
		BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FLAT);
	}

	{
		/* 640x480 content that the user had asked to be scaled to fit the container, without stretch */
		VideoContent vc (0);
		vc._size = dcp::Size(640, 480);
		vc._legacy_ratio = 1.33;
		BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), FOUR_TO_THREE);
	}

	{
		/* 640x480 content that the user had asked to be kept the same size */
		VideoContent vc (0);
		vc._size = dcp::Size(640, 480);
		vc._custom_size = dcp::Size(640, 480);
		BOOST_CHECK_EQUAL (vc.scaled_size(FLAT), dcp::Size(640, 480));
	}
}


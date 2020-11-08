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


/** @file  test/video_level_test.cc
 *  @brief Test that video level ranges are handled correctly.
 *  @ingroup specific
 */


#include "lib/ffmpeg_image_proxy.h"
#include "lib/image.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using boost::shared_ptr;


static
shared_ptr<Image>
grey_image (dcp::Size size, uint8_t pixel)
{
	shared_ptr<Image> grey(new Image(AV_PIX_FMT_RGB24, size, true));
	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = grey->data()[0] + y * grey->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			*p++ = pixel;
			*p++ = pixel;
			*p++ = pixel;
		}
	}

	return grey;
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_full_range_not_changed)
{
	dcp::Size size(640, 480);
	uint8_t const grey_pixel = 128;
	boost::filesystem::path const file = "build/test/ffmpeg_image_full_range_not_changed.png";

	write_image (grey_image(size, grey_pixel), file);

	FFmpegImageProxy proxy (file, VIDEO_RANGE_FULL);
	ImageProxy::Result result = proxy.image ();
	BOOST_REQUIRE (!result.error);

	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = result.image->data()[0] + y * result.image->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			BOOST_REQUIRE (*p++ == grey_pixel);
		}
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_video_range_expanded)
{
	dcp::Size size(640, 480);
	uint8_t const grey_pixel = 128;
	uint8_t const expanded_grey_pixel = static_cast<uint8_t>((grey_pixel - 16) * 256.0 / 219);
	boost::filesystem::path const file = "build/test/ffmpeg_image_video_range_expanded.png";

	write_image (grey_image(size, grey_pixel), file);

	FFmpegImageProxy proxy (file, VIDEO_RANGE_VIDEO);
	ImageProxy::Result result = proxy.image ();
	BOOST_REQUIRE (!result.error);

	for (int y = 0; y < size.height; ++y) {
		uint8_t* p = result.image->data()[0] + y * result.image->stride()[0];
		for (int x = 0; x < size.width; ++x) {
			BOOST_REQUIRE_EQUAL (*p++, expanded_grey_pixel);
		}
	}
}



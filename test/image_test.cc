/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/image_test.cc
 *  @brief Test Image class.
 *  @ingroup selfcontained
 *  @see test/pixel_formats_test.cc
 */


#include "lib/compose.hpp"
#include "lib/image.h"
#include "lib/image_content.h"
#include "lib/image_decoder.h"
#include "lib/image_jpeg.h"
#include "lib/image_png.h"
#include "lib/ffmpeg_image_proxy.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::list;
using std::make_shared;
using std::string;


BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	auto s = new Image (AV_PIX_FMT_RGB24, dcp::Size (50, 50), Image::Alignment::PADDED);
	BOOST_CHECK_EQUAL (s->planes(), 1);
	/* 192 is 150 aligned to the nearest 64 bytes */
	BOOST_CHECK_EQUAL (s->stride()[0], 192);
	BOOST_CHECK_EQUAL (s->line_size()[0], 150);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	auto t = new Image (*s);
	BOOST_CHECK_EQUAL (t->planes(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 192);
	BOOST_CHECK_EQUAL (t->line_size()[0], 150);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK_EQUAL (t->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK_EQUAL (t->stride()[0], s->stride()[0]);

	/* assignment operator */
	auto u = new Image (AV_PIX_FMT_YUV422P, dcp::Size (150, 150), Image::Alignment::COMPACT);
	*u = *s;
	BOOST_CHECK_EQUAL (u->planes(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 192);
	BOOST_CHECK_EQUAL (u->line_size()[0], 150);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK_EQUAL (u->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK_EQUAL (u->stride()[0], s->stride()[0]);

	delete s;
	delete t;
	delete u;
}


BOOST_AUTO_TEST_CASE (compact_image_test)
{
	auto s = new Image (AV_PIX_FMT_RGB24, dcp::Size (50, 50), Image::Alignment::COMPACT);
	BOOST_CHECK_EQUAL (s->planes(), 1);
	BOOST_CHECK_EQUAL (s->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (s->line_size()[0], 50 * 3);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	auto t = new Image (*s);
	BOOST_CHECK_EQUAL (t->planes(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (t->line_size()[0], 50 * 3);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK_EQUAL (t->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK_EQUAL (t->stride()[0], s->stride()[0]);

	/* assignment operator */
	auto u = new Image (AV_PIX_FMT_YUV422P, dcp::Size (150, 150), Image::Alignment::PADDED);
	*u = *s;
	BOOST_CHECK_EQUAL (u->planes(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (u->line_size()[0], 50 * 3);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK_EQUAL (u->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK_EQUAL (u->stride()[0], s->stride()[0]);

	delete s;
	delete t;
	delete u;
}


void
alpha_blend_test_one (AVPixelFormat format, string suffix)
{
	auto proxy = make_shared<FFmpegImageProxy>(TestPaths::private_data() / "prophet_frame.tiff");
	auto raw = proxy->image(Image::Alignment::PADDED).image;
	auto background = raw->convert_pixel_format (dcp::YUVToRGB::REC709, format, Image::Alignment::PADDED, false);

	auto overlay = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size(431, 891), Image::Alignment::PADDED);
	overlay->make_transparent ();

	for (int y = 0; y < 128; ++y) {
		auto p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4 + 2] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	for (int y = 128; y < 256; ++y) {
		auto p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4 + 1] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	for (int y = 256; y < 384; ++y) {
		auto p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	background->alpha_blend (overlay, Position<int> (13, 17));

	auto save = background->convert_pixel_format (dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::COMPACT, false);

	write_image (save, "build/test/image_test_" + suffix + ".png");
	check_image ("build/test/image_test_" + suffix + ".png", TestPaths::private_data() / ("image_test_" + suffix + ".png"));
}


/** Test Image::alpha_blend */
BOOST_AUTO_TEST_CASE (alpha_blend_test)
{
	alpha_blend_test_one (AV_PIX_FMT_RGB24, "rgb24");
	alpha_blend_test_one (AV_PIX_FMT_BGRA, "bgra");
	alpha_blend_test_one (AV_PIX_FMT_RGBA, "rgba");
	alpha_blend_test_one (AV_PIX_FMT_RGB48LE, "rgb48le");
	alpha_blend_test_one (AV_PIX_FMT_YUV420P, "yuv420p");
	alpha_blend_test_one (AV_PIX_FMT_YUV420P10, "yuv420p10");
	alpha_blend_test_one (AV_PIX_FMT_YUV422P10LE, "yuv422p10le");
}


/** Test Image::alpha_blend when the "base" image is XYZ12LE */
BOOST_AUTO_TEST_CASE (alpha_blend_test_onto_xyz)
{
	Image xyz(AV_PIX_FMT_XYZ12LE, dcp::Size(50, 50), Image::Alignment::PADDED);
	xyz.make_black();

	auto overlay = make_shared<Image>(AV_PIX_FMT_RGBA, dcp::Size(8, 8), Image::Alignment::PADDED);
	for (int y = 0; y < 8; ++y) {
		uint8_t* p = overlay->data()[0] + (y * overlay->stride()[0]);
		for (int x = 0; x < 8; ++x) {
			*p++ = 255;
			*p++ = 0;
			*p++ = 0;
			*p++ = 255;
		}
	}

	xyz.alpha_blend(overlay, Position<int>(4, 4));

	for (int y = 0; y < 50; ++y) {
		uint16_t* p = reinterpret_cast<uint16_t*>(xyz.data()[0]) + (y * xyz.stride()[0] / 2);
		for (int x = 0; x < 50; ++x) {
			if (4 <= x && x < 12 && 4 <= y && y < 12) {
				BOOST_REQUIRE_EQUAL(p[0], 45078U);
				BOOST_REQUIRE_EQUAL(p[1], 34939U);
				BOOST_REQUIRE_EQUAL(p[2], 13892U);
			} else {
				BOOST_REQUIRE_EQUAL(p[0], 0U);
				BOOST_REQUIRE_EQUAL(p[1], 0U);
				BOOST_REQUIRE_EQUAL(p[2], 0U);
			}
			p += 3;
		}
	}
}


/** Test merge (list<PositionImage>) with a single image */
BOOST_AUTO_TEST_CASE (merge_test1)
{
	int const stride = 48 * 4;

	auto A = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (48, 48), Image::Alignment::COMPACT);
	A->make_transparent ();
	auto a = A->data()[0];

	for (int y = 0; y < 48; ++y) {
		auto p = a + y * stride;
		for (int x = 0; x < 16; ++x) {
			/* blue */
			p[x * 4] = 255;
			/* opaque */
			p[x * 4 + 3] = 255;
		}
	}

	list<PositionImage> all;
	all.push_back (PositionImage (A, Position<int>(0, 0)));
	auto merged = merge (all, Image::Alignment::COMPACT);

	BOOST_CHECK (merged.position == Position<int>(0, 0));
	BOOST_CHECK_EQUAL (memcmp (merged.image->data()[0], A->data()[0], stride * 48), 0);
}


/** Test merge (list<PositionImage>) with two images */
BOOST_AUTO_TEST_CASE (merge_test2)
{
	auto A = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (48, 1), Image::Alignment::COMPACT);
	A->make_transparent ();
	auto a = A->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* blue */
		a[x * 4] = 255;
		/* opaque */
		a[x * 4 + 3] = 255;
	}

	auto B = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (48, 1), Image::Alignment::COMPACT);
	B->make_transparent ();
	auto b = B->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* red */
		b[(x + 32) * 4 + 2] = 255;
		/* opaque */
		b[(x + 32) * 4 + 3] = 255;
	}

	list<PositionImage> all;
	all.push_back (PositionImage(A, Position<int>(0, 0)));
	all.push_back (PositionImage(B, Position<int>(0, 0)));
	auto merged = merge (all, Image::Alignment::COMPACT);

	BOOST_CHECK (merged.position == Position<int>(0, 0));

	auto m = merged.image->data()[0];

	for (int x = 0; x < 16; ++x) {
		BOOST_CHECK_EQUAL (m[x * 4], 255);
		BOOST_CHECK_EQUAL (m[x * 4 + 3], 255);
		BOOST_CHECK_EQUAL (m[(x + 16) * 4 + 3], 0);
		BOOST_CHECK_EQUAL (m[(x + 32) * 4 + 2], 255);
		BOOST_CHECK_EQUAL (m[(x + 32) * 4 + 3], 255);
	}
}


/** Test Image::crop_scale_window with YUV420P and some windowing */
BOOST_AUTO_TEST_CASE (crop_scale_window_test)
{
	auto proxy = make_shared<FFmpegImageProxy>("test/data/flat_red.png");
	auto raw = proxy->image(Image::Alignment::PADDED).image;
	auto out = raw->crop_scale_window(
		Crop(), dcp::Size(1998, 836), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_YUV420P, VideoRange::FULL, Image::Alignment::PADDED, false
		);
	auto save = out->scale(dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::COMPACT, false);
	write_image(save, "build/test/crop_scale_window_test.png");
	check_image("test/data/crop_scale_window_test.png", "build/test/crop_scale_window_test.png");
}


/** Special cases of Image::crop_scale_window which triggered some valgrind warnings */
BOOST_AUTO_TEST_CASE (crop_scale_window_test2)
{
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(2048, 858), Image::Alignment::PADDED);
	image->crop_scale_window (
		Crop(279, 0, 0, 0), dcp::Size(1069, 448), dcp::Size(1069, 578), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
	image->crop_scale_window (
		Crop(2048, 0, 0, 0), dcp::Size(1069, 448), dcp::Size(1069, 578), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
}


BOOST_AUTO_TEST_CASE (crop_scale_window_test3)
{
	auto proxy = make_shared<FFmpegImageProxy>(TestPaths::private_data() / "player_seek_test_0.png");
	auto xyz = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::PADDED, false);
	auto cropped = xyz->crop_scale_window(
		Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
	write_image(cropped, "build/test/crop_scale_window_test3.png");
	check_image("test/data/crop_scale_window_test3.png", "build/test/crop_scale_window_test3.png");
}


BOOST_AUTO_TEST_CASE (crop_scale_window_test4)
{
	auto proxy = make_shared<FFmpegImageProxy>(TestPaths::private_data() / "player_seek_test_0.png");
	auto xyz = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::PADDED, false);
	auto cropped = xyz->crop_scale_window(
		Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_XYZ12LE, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
	write_image(cropped, "build/test/crop_scale_window_test4.png");
	check_image("test/data/crop_scale_window_test4.png", "build/test/crop_scale_window_test4.png", 35000);
}


BOOST_AUTO_TEST_CASE (crop_scale_window_test5)
{
	auto proxy = make_shared<FFmpegImageProxy>(TestPaths::private_data() / "player_seek_test_0.png");
	auto xyz = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_XYZ12LE, Image::Alignment::PADDED, false);
	auto cropped = xyz->crop_scale_window(
		Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
	write_image(cropped, "build/test/crop_scale_window_test5.png");
	check_image("test/data/crop_scale_window_test5.png", "build/test/crop_scale_window_test5.png");
}


BOOST_AUTO_TEST_CASE (crop_scale_window_test6)
{
	auto proxy = make_shared<FFmpegImageProxy>(TestPaths::private_data() / "player_seek_test_0.png");
	auto xyz = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_XYZ12LE, Image::Alignment::PADDED, false);
	auto cropped = xyz->crop_scale_window(
		Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_XYZ12LE, VideoRange::FULL, Image::Alignment::COMPACT, false
		);
	write_image(cropped, "build/test/crop_scale_window_test6.png");
	check_image("test/data/crop_scale_window_test6.png", "build/test/crop_scale_window_test6.png", 35000);
}


/** Test some small crops with an image that shows up errors in registration of the YUV planes (#1872) */
BOOST_AUTO_TEST_CASE (crop_scale_window_test7)
{
	using namespace boost::filesystem;
	for (int left_crop = 0; left_crop < 8; ++left_crop) {
		auto proxy = make_shared<FFmpegImageProxy>("test/data/rgb_grey_testcard.png");
		auto yuv = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_YUV420P, Image::Alignment::PADDED, false);
		int rounded = left_crop - (left_crop % 2);
		auto cropped = yuv->crop_scale_window(
			Crop(left_crop, 0, 0, 0),
			dcp::Size(1998 - rounded, 1080),
			dcp::Size(1998 - rounded, 1080),
			dcp::YUVToRGB::REC709,
			VideoRange::VIDEO,
			AV_PIX_FMT_RGB24,
			VideoRange::VIDEO,
			Image::Alignment::PADDED,
			false
			);
		path file = String::compose("crop_scale_window_test7-%1.png", left_crop);
		write_image(cropped, path("build") / "test" / file);
		check_image(path("test") / "data" / file, path("build") / "test" / file, 10);
	}
}


BOOST_AUTO_TEST_CASE (crop_scale_window_test8)
{
	using namespace boost::filesystem;

	auto image = make_shared<Image>(AV_PIX_FMT_YUV420P, dcp::Size(800, 600), Image::Alignment::PADDED);
	memset(image->data()[0], 41, image->stride()[0] * 600);
	memset(image->data()[1], 240, image->stride()[1] * 300);
	memset(image->data()[2], 41, image->stride()[2] * 300);
	auto scaled = image->crop_scale_window(
		Crop(), dcp::Size(1435, 1080), dcp::Size(1998, 1080), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_YUV420P, VideoRange::FULL, Image::Alignment::PADDED, false
		);
	auto file = "crop_scale_window_test8.png";
	write_image(scaled->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, Image::Alignment::COMPACT, false), path("build") / "test" / file);
	check_image(path("test") / "data" / file, path("build") / "test" / file, 10);
}


BOOST_AUTO_TEST_CASE (as_png_test)
{
	auto proxy = make_shared<FFmpegImageProxy>("test/data/3d_test/000001.png");
	auto image_rgb = proxy->image(Image::Alignment::PADDED).image;
	auto image_bgr = image_rgb->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_BGRA, Image::Alignment::PADDED, false);
	image_as_png(image_rgb).write ("build/test/as_png_rgb.png");
	image_as_png(image_bgr).write ("build/test/as_png_bgr.png");

	check_image ("test/data/3d_test/000001.png", "build/test/as_png_rgb.png");
	check_image ("test/data/3d_test/000001.png", "build/test/as_png_bgr.png");
}


BOOST_AUTO_TEST_CASE (as_jpeg_test)
{
	auto proxy = make_shared<FFmpegImageProxy>("test/data/3d_test/000001.png");
	auto image_rgb = proxy->image(Image::Alignment::PADDED).image;
	auto image_bgr = image_rgb->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_BGRA, Image::Alignment::PADDED, false);
	image_as_jpeg(image_rgb, 60).write("build/test/as_jpeg_rgb.jpeg");
	image_as_jpeg(image_bgr, 60).write("build/test/as_jpeg_bgr.jpeg");

	check_image ("test/data/as_jpeg_rgb.jpeg", "build/test/as_jpeg_rgb.jpeg");
	check_image ("test/data/as_jpeg_bgr.jpeg", "build/test/as_jpeg_bgr.jpeg");
}


/* Very dumb test to fade black to make sure it stays black */
static void
fade_test_format_black (AVPixelFormat f, string name)
{
	Image yuv (f, dcp::Size(640, 480), Image::Alignment::PADDED);
	yuv.make_black ();
	yuv.fade (0);
	string const filename = "fade_test_black_" + name + ".png";
	image_as_png(yuv.convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGBA, Image::Alignment::PADDED, false)).write("build/test/" + filename);
	check_image ("test/data/" + filename, "build/test/" + filename);
}


/* Fade red to make sure it stays red */
static void
fade_test_format_red (AVPixelFormat f, float amount, string name)
{
	auto proxy = make_shared<FFmpegImageProxy>("test/data/flat_red.png");
	auto red = proxy->image(Image::Alignment::PADDED).image->convert_pixel_format(dcp::YUVToRGB::REC709, f, Image::Alignment::PADDED, false);
	red->fade (amount);
	string const filename = "fade_test_red_" + name + ".png";
	image_as_png(red->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGBA, Image::Alignment::PADDED, false)).write("build/test/" + filename);
	check_image ("test/data/" + filename, "build/test/" + filename);
}


BOOST_AUTO_TEST_CASE (fade_test)
{
	fade_test_format_black (AV_PIX_FMT_YUV420P,   "yuv420p");
	fade_test_format_black (AV_PIX_FMT_YUV422P10, "yuv422p10");
	fade_test_format_black (AV_PIX_FMT_RGB24,     "rgb24");
	fade_test_format_black (AV_PIX_FMT_XYZ12LE,   "xyz12le");
	fade_test_format_black (AV_PIX_FMT_RGB48LE,   "rgb48le");

	fade_test_format_red   (AV_PIX_FMT_YUV420P,   0,   "yuv420p_0");
	fade_test_format_red   (AV_PIX_FMT_YUV420P,   0.5, "yuv420p_50");
	fade_test_format_red   (AV_PIX_FMT_YUV420P,   1,   "yuv420p_100");
	fade_test_format_red   (AV_PIX_FMT_YUV422P10, 0,   "yuv422p10_0");
	fade_test_format_red   (AV_PIX_FMT_YUV422P10, 0.5, "yuv422p10_50");
	fade_test_format_red   (AV_PIX_FMT_YUV422P10, 1,   "yuv422p10_100");
	fade_test_format_red   (AV_PIX_FMT_RGB24,     0,   "rgb24_0");
	fade_test_format_red   (AV_PIX_FMT_RGB24,     0.5, "rgb24_50");
	fade_test_format_red   (AV_PIX_FMT_RGB24,     1,   "rgb24_100");
	fade_test_format_red   (AV_PIX_FMT_XYZ12LE,   0,   "xyz12le_0");
	fade_test_format_red   (AV_PIX_FMT_XYZ12LE,   0.5, "xyz12le_50");
	fade_test_format_red   (AV_PIX_FMT_XYZ12LE,   1,   "xyz12le_100");
	fade_test_format_red   (AV_PIX_FMT_RGB48LE,   0,   "rgb48le_0");
	fade_test_format_red   (AV_PIX_FMT_RGB48LE,   0.5, "rgb48le_50");
	fade_test_format_red   (AV_PIX_FMT_RGB48LE,   1,   "rgb48le_100");
}


BOOST_AUTO_TEST_CASE (make_black_test)
{
	dcp::Size in_size (512, 512);
	dcp::Size out_size (1024, 1024);

	list<AVPixelFormat> pix_fmts = {
		AV_PIX_FMT_RGB24, // 2
		AV_PIX_FMT_ARGB,
		AV_PIX_FMT_RGBA,
		AV_PIX_FMT_ABGR,
		AV_PIX_FMT_BGRA,
		AV_PIX_FMT_YUV420P, // 0
		AV_PIX_FMT_YUV411P,
		AV_PIX_FMT_YUV422P10LE,
		AV_PIX_FMT_YUV422P16LE,
		AV_PIX_FMT_YUV444P9LE,
		AV_PIX_FMT_YUV444P9BE,
		AV_PIX_FMT_YUV444P10LE,
		AV_PIX_FMT_YUV444P10BE,
		AV_PIX_FMT_UYVY422,
		AV_PIX_FMT_YUVJ420P,
		AV_PIX_FMT_YUVJ422P,
		AV_PIX_FMT_YUVJ444P,
		AV_PIX_FMT_YUVA420P9BE,
		AV_PIX_FMT_YUVA422P9BE,
		AV_PIX_FMT_YUVA444P9BE,
		AV_PIX_FMT_YUVA420P9LE,
		AV_PIX_FMT_YUVA422P9LE,
		AV_PIX_FMT_YUVA444P9LE,
		AV_PIX_FMT_YUVA420P10BE,
		AV_PIX_FMT_YUVA422P10BE,
		AV_PIX_FMT_YUVA444P10BE,
		AV_PIX_FMT_YUVA420P10LE,
		AV_PIX_FMT_YUVA422P10LE,
		AV_PIX_FMT_YUVA444P10LE,
		AV_PIX_FMT_YUVA420P16BE,
		AV_PIX_FMT_YUVA422P16BE,
		AV_PIX_FMT_YUVA444P16BE,
		AV_PIX_FMT_YUVA420P16LE,
		AV_PIX_FMT_YUVA422P16LE,
		AV_PIX_FMT_YUVA444P16LE,
		AV_PIX_FMT_RGB555LE, // 46
	};

	int N = 0;
	for (auto i: pix_fmts) {
		auto foo = make_shared<Image>(i, in_size, Image::Alignment::PADDED);
		foo->make_black ();
		auto bar = foo->scale (out_size, dcp::YUVToRGB::REC601, AV_PIX_FMT_RGB24, Image::Alignment::PADDED, false);

		uint8_t* p = bar->data()[0];
		for (int y = 0; y < bar->size().height; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < bar->line_size()[0]; ++x) {
				if (*q != 0) {
					std::cerr << "x=" << x << ", (x%3)=" << (x%3) << "\n";
				}
				BOOST_CHECK_EQUAL (*q++, 0);
			}
			p += bar->stride()[0];
		}

		++N;
	}
}


BOOST_AUTO_TEST_CASE (make_part_black_test)
{
	auto proxy = make_shared<FFmpegImageProxy>("test/data/flat_red.png");
	auto original = proxy->image(Image::Alignment::PADDED).image;

	list<AVPixelFormat> pix_fmts = {
		AV_PIX_FMT_RGB24,
		AV_PIX_FMT_ARGB,
		AV_PIX_FMT_RGBA,
		AV_PIX_FMT_ABGR,
		AV_PIX_FMT_BGRA,
		AV_PIX_FMT_YUV420P,
		AV_PIX_FMT_YUV422P10LE,
	};

	list<std::pair<int, int>> positions = {
		{ 0, 256 },
		{ 128, 64 },
	};

	int N = 0;
	for (auto i: pix_fmts) {
		for (auto j: positions) {
			auto foo = original->convert_pixel_format(dcp::YUVToRGB::REC601, i, Image::Alignment::PADDED, false);
			foo->make_part_black (j.first, j.second);
			auto bar = foo->convert_pixel_format (dcp::YUVToRGB::REC601, AV_PIX_FMT_RGB24, Image::Alignment::PADDED, false);

			auto p = bar->data()[0];
			for (int y = 0; y < bar->size().height; ++y) {
				auto q = p;
				for (int x = 0; x < bar->size().width; ++x) {
					int r = *q++;
					int g = *q++;
					int b = *q++;
					if (x >= j.first && x < (j.first + j.second)) {
						BOOST_CHECK_MESSAGE (
							r < 3, "red=" << static_cast<int>(r) << " at (" << x << "," << y << ") format " << i << " from " << j.first << " width " << j.second
							);
					} else {
						BOOST_CHECK_MESSAGE (
							r >= 252, "red=" << static_cast<int>(r) << " at (" << x << "," << y << ") format " << i << " from " << j.first << " width " << j.second
							);

					}
					BOOST_CHECK_MESSAGE (
						g == 0, "green=" << static_cast<int>(g) << " at (" << x << "," << y << ") format " << i << " from " << j.first << " width " << j.second
						);
					BOOST_CHECK_MESSAGE (
						b == 0, "blue=" << static_cast<int>(b) << " at (" << x << "," << y << ") format " << i << " from " << j.first << " width " << j.second
						);
				}
				p += bar->stride()[0];
			}

			++N;
		}
	}
}


/** Make sure the image isn't corrupted if it is cropped too much.  This can happen when a
 *  filler 128x128 black frame is emitted from the FFmpegDecoder and the overall crop in either direction
 *  is greater than 128 pixels.
 */
BOOST_AUTO_TEST_CASE (over_crop_test)
{
	auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size(128, 128), Image::Alignment::PADDED);
	image->make_black ();
	auto scaled = image->crop_scale_window (
		Crop(0, 0, 128, 128), dcp::Size(1323, 565), dcp::Size(1349, 565), dcp::YUVToRGB::REC709, VideoRange::FULL, AV_PIX_FMT_RGB24, VideoRange::FULL, Image::Alignment::PADDED, true
		);
	string const filename = "over_crop_test.png";
	write_image (scaled, "build/test/" + filename);
	check_image ("test/data/" + filename, "build/test/" + filename);
}

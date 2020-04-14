/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
 *  @see test/make_black_test.cc, test/pixel_formats_test.cc
 */

#include "lib/image.h"
#include "lib/ffmpeg_image_proxy.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	Image* s = new Image (AV_PIX_FMT_RGB24, dcp::Size (50, 50), true);
	BOOST_CHECK_EQUAL (s->planes(), 1);
	/* 160 is 150 aligned to the nearest 32 bytes */
	BOOST_CHECK_EQUAL (s->stride()[0], 160);
	BOOST_CHECK_EQUAL (s->line_size()[0], 150);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	Image* t = new Image (*s);
	BOOST_CHECK_EQUAL (t->planes(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 160);
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
	Image* u = new Image (AV_PIX_FMT_YUV422P, dcp::Size (150, 150), false);
	*u = *s;
	BOOST_CHECK_EQUAL (u->planes(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 160);
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
	Image* s = new Image (AV_PIX_FMT_RGB24, dcp::Size (50, 50), false);
	BOOST_CHECK_EQUAL (s->planes(), 1);
	BOOST_CHECK_EQUAL (s->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (s->line_size()[0], 50 * 3);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	Image* t = new Image (*s);
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
	Image* u = new Image (AV_PIX_FMT_YUV422P, dcp::Size (150, 150), true);
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
	shared_ptr<FFmpegImageProxy> proxy (new FFmpegImageProxy (TestPaths::private_data / "prophet_frame.tiff"));
	shared_ptr<Image> raw = proxy->image().image;
	shared_ptr<Image> background = raw->convert_pixel_format (dcp::YUV_TO_RGB_REC709, format, true, false);

	shared_ptr<Image> overlay (new Image (AV_PIX_FMT_BGRA, dcp::Size(431, 891), true));
	overlay->make_transparent ();

	for (int y = 0; y < 128; ++y) {
		uint8_t* p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4 + 2] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	for (int y = 128; y < 256; ++y) {
		uint8_t* p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4 + 1] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	for (int y = 256; y < 384; ++y) {
		uint8_t* p = overlay->data()[0] + y * overlay->stride()[0];
		for (int x = 0; x < 128; ++x) {
			p[x * 4] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	background->alpha_blend (overlay, Position<int> (13, 17));

	shared_ptr<Image> save = background->convert_pixel_format (dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGB24, false, false);

	write_image (save, "build/test/image_test_" + suffix + ".png", "RGB");
	check_image ("build/test/image_test_" + suffix + ".png", TestPaths::private_data / ("image_test_" + suffix + ".png"));
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

/** Test merge (list<PositionImage>) with a single image */
BOOST_AUTO_TEST_CASE (merge_test1)
{
	int const stride = 48 * 4;

	shared_ptr<Image> A (new Image (AV_PIX_FMT_BGRA, dcp::Size (48, 48), false));
	A->make_transparent ();
	uint8_t* a = A->data()[0];

	for (int y = 0; y < 48; ++y) {
		uint8_t* p = a + y * stride;
		for (int x = 0; x < 16; ++x) {
			/* blue */
			p[x * 4] = 255;
			/* opaque */
			p[x * 4 + 3] = 255;
		}
	}

	list<PositionImage> all;
	all.push_back (PositionImage (A, Position<int> (0, 0)));
	PositionImage merged = merge (all);

	BOOST_CHECK (merged.position == Position<int> (0, 0));
	BOOST_CHECK_EQUAL (memcmp (merged.image->data()[0], A->data()[0], stride * 48), 0);
}

/** Test merge (list<PositionImage>) with two images */
BOOST_AUTO_TEST_CASE (merge_test2)
{
	shared_ptr<Image> A (new Image (AV_PIX_FMT_BGRA, dcp::Size (48, 1), false));
	A->make_transparent ();
	uint8_t* a = A->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* blue */
		a[x * 4] = 255;
		/* opaque */
		a[x * 4 + 3] = 255;
	}

	shared_ptr<Image> B (new Image (AV_PIX_FMT_BGRA, dcp::Size (48, 1), false));
	B->make_transparent ();
	uint8_t* b = B->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* red */
		b[(x + 32) * 4 + 2] = 255;
		/* opaque */
		b[(x + 32) * 4 + 3] = 255;
	}

	list<PositionImage> all;
	all.push_back (PositionImage (A, Position<int> (0, 0)));
	all.push_back (PositionImage (B, Position<int> (0, 0)));
	PositionImage merged = merge (all);

	BOOST_CHECK (merged.position == Position<int> (0, 0));

	uint8_t* m = merged.image->data()[0];

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
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/flat_red.png"));
	shared_ptr<Image> raw = proxy->image().image;
	shared_ptr<Image> out = raw->crop_scale_window(Crop(), dcp::Size(1998, 836), dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_YUV420P, true, false);
	shared_ptr<Image> save = out->scale(dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGB24, false, false);
	write_image(save, "build/test/crop_scale_window_test.png", "RGB");
	check_image("test/data/crop_scale_window_test.png", "build/test/crop_scale_window_test.png");
}

/** Special cases of Image::crop_scale_window which triggered some valgrind warnings */
BOOST_AUTO_TEST_CASE (crop_scale_window_test2)
{
	shared_ptr<Image> image (new Image(AV_PIX_FMT_XYZ12LE, dcp::Size(2048, 858), true));
	image->crop_scale_window (Crop(279, 0, 0, 0), dcp::Size(1069, 448), dcp::Size(1069, 578), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_RGB24, false, false);
	image->crop_scale_window (Crop(2048, 0, 0, 0), dcp::Size(1069, 448), dcp::Size(1069, 578), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_RGB24, false, false);
}

BOOST_AUTO_TEST_CASE (crop_scale_window_test3)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/player_seek_test_0.png"));
	shared_ptr<Image> xyz = proxy->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGB24, true, false);
	shared_ptr<Image> cropped = xyz->crop_scale_window(Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_RGB24, false, false);
	write_image(cropped, "build/test/crop_scale_window_test3.png", "RGB", MagickCore::CharPixel);
	check_image("test/data/crop_scale_window_test3.png", "build/test/crop_scale_window_test3.png");
}

BOOST_AUTO_TEST_CASE (crop_scale_window_test4)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/player_seek_test_0.png"));
	shared_ptr<Image> xyz = proxy->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGB24, true, false);
	shared_ptr<Image> cropped = xyz->crop_scale_window(Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_XYZ12LE, false, false);
	write_image(cropped, "build/test/crop_scale_window_test4.png", "RGB", MagickCore::ShortPixel);
	check_image("test/data/crop_scale_window_test4.png", "build/test/crop_scale_window_test4.png");
}

BOOST_AUTO_TEST_CASE (crop_scale_window_test5)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/player_seek_test_0.png"));
	shared_ptr<Image> xyz = proxy->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_XYZ12LE, true, false);
	shared_ptr<Image> cropped = xyz->crop_scale_window(Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_RGB24, false, false);
	write_image(cropped, "build/test/crop_scale_window_test5.png", "RGB", MagickCore::CharPixel);
	check_image("test/data/crop_scale_window_test5.png", "build/test/crop_scale_window_test5.png");
}

BOOST_AUTO_TEST_CASE (crop_scale_window_test6)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/player_seek_test_0.png"));
	shared_ptr<Image> xyz = proxy->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_XYZ12LE, true, false);
	shared_ptr<Image> cropped = xyz->crop_scale_window(Crop(512, 0, 0, 0), dcp::Size(1486, 1080), dcp::Size(1998, 1080), dcp::YUV_TO_RGB_REC709, VIDEO_RANGE_FULL, AV_PIX_FMT_XYZ12LE, false, false);
	write_image(cropped, "build/test/crop_scale_window_test6.png", "RGB", MagickCore::ShortPixel);
	check_image("test/data/crop_scale_window_test6.png", "build/test/crop_scale_window_test6.png");
}

BOOST_AUTO_TEST_CASE (as_png_test)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/3d_test/000001.png"));
	shared_ptr<Image> image_rgb = proxy->image().image;
	shared_ptr<Image> image_bgr = image_rgb->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_BGRA, true, false);
	image_rgb->as_png().write ("build/test/as_png_rgb.png");
	image_bgr->as_png().write ("build/test/as_png_bgr.png");

	check_image ("test/data/3d_test/000001.png", "build/test/as_png_rgb.png");
	check_image ("test/data/3d_test/000001.png", "build/test/as_png_bgr.png");
}

/* Very dumb test to fade black to make sure it stays black */
static void
fade_test_format_black (AVPixelFormat f, string name)
{
	Image yuv (f, dcp::Size(640, 480), true);
	yuv.make_black ();
	yuv.fade (0);
	string const filename = "fade_test_black_" + name + ".png";
	yuv.convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGBA, true, false)->as_png().write("build/test/" + filename);
	check_image ("test/data/" + filename, "build/test/" + filename);
}

/* Fade red to make sure it stays red */
static void
fade_test_format_red (AVPixelFormat f, float amount, string name)
{
	shared_ptr<FFmpegImageProxy> proxy(new FFmpegImageProxy("test/data/flat_red.png"));
	shared_ptr<Image> red = proxy->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, f, true, false);
	red->fade (amount);
	string const filename = "fade_test_red_" + name + ".png";
	red->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGBA, true, false)->as_png().write("build/test/" + filename);
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

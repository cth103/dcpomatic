/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
#include <Magick++.h>
#include "lib/image.h"
#include "lib/scaler.h"

using std::string;
using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	Image* s = new Image (PIX_FMT_RGB24, libdcp::Size (50, 50), true);
	BOOST_CHECK_EQUAL (s->components(), 1);
	/* 160 is 150 aligned to the nearest 32 bytes */
	BOOST_CHECK_EQUAL (s->stride()[0], 160);
	BOOST_CHECK_EQUAL (s->line_size()[0], 150);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	Image* t = new Image (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 160);
	BOOST_CHECK_EQUAL (t->line_size()[0], 150);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	Image* u = new Image (PIX_FMT_YUV422P, libdcp::Size (150, 150), false);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 160);
	BOOST_CHECK_EQUAL (u->line_size()[0], 150);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (compact_image_test)
{
	Image* s = new Image (PIX_FMT_RGB24, libdcp::Size (50, 50), false);
	BOOST_CHECK_EQUAL (s->components(), 1);
	BOOST_CHECK_EQUAL (s->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (s->line_size()[0], 50 * 3);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	Image* t = new Image (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (t->line_size()[0], 50 * 3);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	Image* u = new Image (PIX_FMT_YUV422P, libdcp::Size (150, 150), true);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (u->line_size()[0], 50 * 3);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (crop_image_test)
{
	/* This was to check out a bug with valgrind, and is probably not very useful */
	shared_ptr<Image> image (new Image (PIX_FMT_YUV420P, libdcp::Size (16, 16), true));
	image->make_black ();
	Crop crop;
	crop.top = 3;
	image->crop (crop, false);
}

/* Test cropping of a YUV 4:2:0 image by 1 pixel, which used to fail because
   the U/V copying was not rounded up to the next sample.
*/
BOOST_AUTO_TEST_CASE (crop_image_test2)
{
	/* Here's a 1998 x 1080 image which is black */
	shared_ptr<Image> image (new Image (PIX_FMT_YUV420P, libdcp::Size (1998, 1080), true));
	image->make_black ();

	/* Crop it by 1 pixel */
	Crop crop;
	crop.left = 1;
	image = image->crop (crop, true);

	/* Convert it back to RGB to make comparison to black easier */
	image = image->scale (image->size(), Scaler::from_id ("bicubic"), PIX_FMT_RGB24, true);

	/* Check that its still black after the crop */
	uint8_t* p = image->data()[0];
	for (int y = 0; y < image->size().height; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < image->size().width; ++x) {
			BOOST_CHECK_EQUAL (*q++, 0);
			BOOST_CHECK_EQUAL (*q++, 0);
			BOOST_CHECK_EQUAL (*q++, 0);
		}
		p += image->stride()[0];
	}
}

static
boost::shared_ptr<Image>
read_file (string file)
{
	Magick::Image magick_image (file.c_str ());
	libdcp::Size size (magick_image.columns(), magick_image.rows());

	boost::shared_ptr<Image> image (new Image (PIX_FMT_RGB24, size, true));

	using namespace MagickCore;
	
	uint8_t* p = image->data()[0];
	for (int y = 0; y < size.height; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = magick_image.pixelColor (x, y);
			*q++ = c.redQuantum() * 255 / QuantumRange;
			*q++ = c.greenQuantum() * 255 / QuantumRange;
			*q++ = c.blueQuantum() * 255 / QuantumRange;
		}
		p += image->stride()[0];
	}

	return image;
}

static
void
write_file (shared_ptr<Image> image, string file)
{
	using namespace MagickCore;
	
	Magick::Image magick_image (Magick::Geometry (image->size().width, image->size().height), Magick::Color (0, 0, 0));
	uint8_t*p = image->data()[0];
	for (int y = 0; y < image->size().height; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < image->size().width; ++x) {
			Magick::Color c (q[0] * QuantumRange / 256, q[1] * QuantumRange / 256, q[2] * QuantumRange / 256);
			magick_image.pixelColor (x, y, c);
			q += 3;
		}
		p += image->stride()[0];
	}
	
	magick_image.write (file.c_str ());
}

static
void
crop_scale_window_single (AVPixelFormat in_format, libdcp::Size in_size, Crop crop, libdcp::Size inter_size, libdcp::Size out_size)
{
	/* Set up our test image */
	shared_ptr<Image> test (new Image (in_format, in_size, true));
	uint8_t n = 0;
	for (int c = 0; c < test->components(); ++c) {
		uint8_t* p = test->data()[c];
		for (int y = 0; y < test->lines(c); ++y) {
			for (int x = 0; x < test->stride()[c]; ++x) {
				*p++ = n++;
			}
		}
	}
				
	/* Convert using separate methods */
	boost::shared_ptr<Image> sep = test->crop (crop, true);
	sep = sep->scale (inter_size, Scaler::from_id ("bicubic"), PIX_FMT_RGB24, true);
	boost::shared_ptr<Image> sep_container (new Image (PIX_FMT_RGB24, out_size, true));
	sep_container->make_black ();
	sep_container->copy (sep, Position<int> ((out_size.width - inter_size.width) / 2, (out_size.height - inter_size.height) / 2));

	/* Convert using the all-in-one method */
	shared_ptr<Image> all = test->crop_scale_window (crop, inter_size, out_size, Scaler::from_id ("bicubic"), PIX_FMT_RGB24, true);

	/* Compare */
	BOOST_CHECK_EQUAL (sep_container->size().width, all->size().width);
	BOOST_CHECK_EQUAL (sep_container->size().height, all->size().height);

	/* Assuming RGB on these */
	BOOST_CHECK_EQUAL (sep_container->components(), 1);
	BOOST_CHECK_EQUAL (all->components(), 1);

	uint8_t* p = sep_container->data()[0];
	uint8_t* q = all->data()[0];
	for (int y = 0; y < all->size().height; ++y) {
		uint8_t* pp = p;
		uint8_t* qq = q;
		for (int x = 0; x < all->size().width * 3; ++x) {
			BOOST_CHECK_EQUAL (*pp++, *qq++);
		}
		p += sep_container->stride()[0];
		q += all->stride()[0];
	}
}

/** Test Image::crop_scale_window against separate calls to crop/scale/copy */
BOOST_AUTO_TEST_CASE (crop_scale_window_test)
{
	crop_scale_window_single (AV_PIX_FMT_YUV422P, libdcp::Size (640, 480), Crop (), libdcp::Size (640, 480), libdcp::Size (640, 480));
	crop_scale_window_single (AV_PIX_FMT_YUV422P, libdcp::Size (640, 480), Crop (2, 4, 6, 8), libdcp::Size (640, 480), libdcp::Size (640, 480));
	crop_scale_window_single (AV_PIX_FMT_YUV422P, libdcp::Size (640, 480), Crop (2, 4, 6, 8), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
	crop_scale_window_single (AV_PIX_FMT_YUV422P, libdcp::Size (640, 480), Crop (1, 4, 6, 8), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
	crop_scale_window_single (AV_PIX_FMT_YUV420P, libdcp::Size (640, 480), Crop (16, 16, 0, 0), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
	crop_scale_window_single (AV_PIX_FMT_YUV420P, libdcp::Size (640, 480), Crop (16, 3, 3, 0), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
	crop_scale_window_single (AV_PIX_FMT_RGB24, libdcp::Size (1000, 800), Crop (0, 0, 0, 0), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
	crop_scale_window_single (AV_PIX_FMT_RGB24, libdcp::Size (1000, 800), Crop (55, 0, 1, 9), libdcp::Size (1920, 1080), libdcp::Size (1998, 1080));
}

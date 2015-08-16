/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  test/image_test.cc
 *  @brief Tests of the Image class.
 *
 *  @see test/make_black_test.cc, test/pixel_formats_test.cc
 */

#include <boost/test/unit_test.hpp>
#include <Magick++.h>
#include "lib/image.h"

using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	Image* s = new Image (PIX_FMT_RGB24, dcp::Size (50, 50), true);
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
	BOOST_CHECK_EQUAL (t->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK_EQUAL (t->stride()[0], s->stride()[0]);

	/* assignment operator */
	Image* u = new Image (PIX_FMT_YUV422P, dcp::Size (150, 150), false);
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
	BOOST_CHECK_EQUAL (u->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK_EQUAL (u->stride()[0], s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (compact_image_test)
{
	Image* s = new Image (PIX_FMT_RGB24, dcp::Size (50, 50), false);
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
	BOOST_CHECK_EQUAL (t->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK_EQUAL (t->stride()[0], s->stride()[0]);

	/* assignment operator */
	Image* u = new Image (PIX_FMT_YUV422P, dcp::Size (150, 150), true);
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
	BOOST_CHECK_EQUAL (u->line_size()[0], s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK_EQUAL (u->stride()[0], s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

/** Test Image::alpha_blend */
BOOST_AUTO_TEST_CASE (alpha_blend_test)
{
	int const stride = 48 * 4;

	shared_ptr<Image> A (new Image (AV_PIX_FMT_RGBA, dcp::Size (48, 48), false));
	A->make_black ();
	uint8_t* a = A->data()[0];

	for (int y = 0; y < 48; ++y) {
		uint8_t* p = a + y * stride;
		for (int x = 0; x < 16; ++x) {
			p[x * 4] = 255;
			p[(x + 16) * 4 + 1] = 255;
			p[(x + 32) * 4 + 2] = 255;
		}
	}

	shared_ptr<Image> B (new Image (AV_PIX_FMT_RGBA, dcp::Size (48, 48), true));
	B->make_transparent ();
	uint8_t* b = B->data()[0];

	for (int y = 32; y < 48; ++y) {
		uint8_t* p = b + y * stride;
		for (int x = 0; x < 48; ++x) {
			p[x * 4] = 255;
			p[x * 4 + 1] = 255;
			p[x * 4 + 2] = 255;
			p[x * 4 + 3] = 255;
		}
	}

	A->alpha_blend (B, Position<int> (0, 0));

	for (int y = 0; y < 32; ++y) {
		uint8_t* p = a + y * stride;
		for (int x = 0; x < 16; ++x) {
			BOOST_CHECK_EQUAL (p[x * 4], 255);
			BOOST_CHECK_EQUAL (p[(x + 16) * 4 + 1], 255);
			BOOST_CHECK_EQUAL (p[(x + 32) * 4 + 2], 255);
		}
	}

	for (int y = 32; y < 48; ++y) {
		uint8_t* p = a + y * stride;
		for (int x = 0; x < 48; ++x) {
			BOOST_CHECK_EQUAL (p[x * 4], 255);
			BOOST_CHECK_EQUAL (p[x * 4 + 1], 255);
			BOOST_CHECK_EQUAL (p[x * 4 + 2], 255);
			BOOST_CHECK_EQUAL (p[x * 4 + 3], 255);
		}
	}
}

/** Test merge (list<PositionImage>) with a single image */
BOOST_AUTO_TEST_CASE (merge_test1)
{
	int const stride = 48 * 4;

	shared_ptr<Image> A (new Image (AV_PIX_FMT_RGBA, dcp::Size (48, 48), false));
	A->make_transparent ();
	uint8_t* a = A->data()[0];

	for (int y = 0; y < 48; ++y) {
		uint8_t* p = a + y * stride;
		for (int x = 0; x < 16; ++x) {
			/* red */
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
	shared_ptr<Image> A (new Image (AV_PIX_FMT_RGBA, dcp::Size (48, 1), false));
	A->make_transparent ();
	uint8_t* a = A->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* red */
		a[x * 4] = 255;
		/* opaque */
		a[x * 4 + 3] = 255;
	}

	shared_ptr<Image> B (new Image (AV_PIX_FMT_RGBA, dcp::Size (48, 1), false));
	B->make_transparent ();
	uint8_t* b = B->data()[0];
	for (int x = 0; x < 16; ++x) {
		/* blue */
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

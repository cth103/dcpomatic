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

#include <boost/test/unit_test.hpp>
#include <list>
extern "C" {
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
}
#include "lib/image.h"

using std::list;
using std::cout;

struct Case
{
	Case (AVPixelFormat f, int c, int l0, int l1, int l2, float b0, float b1, float b2)
		: format(f)
		, components(c)
	{
		lines[0] = l0;
		lines[1] = l1;
		lines[2] = l2;
		bpp[0] = b0;
		bpp[1] = b1;
		bpp[2] = b2;
	}
	
	AVPixelFormat format;
	int components;
	int lines[3];
	float bpp[3];
};


BOOST_AUTO_TEST_CASE (pixel_formats_test)
{
	list<Case> cases;
	cases.push_back(Case(AV_PIX_FMT_RGB24,       1, 480, 480, 480, 3, 0,   0  ));
	cases.push_back(Case(AV_PIX_FMT_RGBA,        1, 480, 480, 480, 4, 0,   0  ));
	cases.push_back(Case(AV_PIX_FMT_YUV420P,     3, 480, 240, 240, 1, 0.5, 0.5));
	cases.push_back(Case(AV_PIX_FMT_YUV422P,     3, 480, 480, 480, 1, 0.5, 0.5));
	cases.push_back(Case(AV_PIX_FMT_YUV422P10LE, 3, 480, 480, 480, 2, 1,   1  ));
	cases.push_back(Case(AV_PIX_FMT_YUV422P16LE, 3, 480, 480, 480, 2, 1,   1  ));
	cases.push_back(Case(AV_PIX_FMT_UYVY422,     1, 480, 480, 480, 2, 0,   0  ));
	cases.push_back(Case(AV_PIX_FMT_YUV444P,     3, 480, 480, 480, 1, 1,   1  ));
	cases.push_back(Case(AV_PIX_FMT_YUV444P9BE,  3, 480, 480, 480, 2, 2,   2  ));
	cases.push_back(Case(AV_PIX_FMT_YUV444P9LE,  3, 480, 480, 480, 2, 2,   2  ));
	cases.push_back(Case(AV_PIX_FMT_YUV444P10BE, 3, 480, 480, 480, 2, 2,   2  ));
	cases.push_back(Case(AV_PIX_FMT_YUV444P10LE, 3, 480, 480, 480, 2, 2,   2  ));

	for (list<Case>::iterator i = cases.begin(); i != cases.end(); ++i) {
		AVFrame* f = av_frame_alloc ();
		f->width = 640;
		f->height = 480;
		f->format = static_cast<int> (i->format);
		av_frame_get_buffer (f, true);
		Image t (f);
		BOOST_CHECK_EQUAL(t.components(), i->components);
		BOOST_CHECK_EQUAL(t.lines(0), i->lines[0]);
		BOOST_CHECK_EQUAL(t.lines(1), i->lines[1]);
		BOOST_CHECK_EQUAL(t.lines(2), i->lines[2]);
		BOOST_CHECK_EQUAL(t.bytes_per_pixel(0), i->bpp[0]);
		BOOST_CHECK_EQUAL(t.bytes_per_pixel(1), i->bpp[1]);
		BOOST_CHECK_EQUAL(t.bytes_per_pixel(2), i->bpp[2]);
	}
}

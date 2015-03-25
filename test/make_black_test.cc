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

/** @file  test/make_black_test.cc
 *  @brief Check that Image::make_black works, and doesn't use values which crash
 *  sws_scale().
 *
 *  @see test/image_test.cc
 */

#include <boost/test/unit_test.hpp>
#include <dcp/util.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include "lib/image.h"

using std::list;

BOOST_AUTO_TEST_CASE (make_black_test)
{
	dcp::Size in_size (512, 512);
	dcp::Size out_size (1024, 1024);

	list<AVPixelFormat> pix_fmts;
	pix_fmts.push_back (AV_PIX_FMT_RGB24); // 2
	pix_fmts.push_back (AV_PIX_FMT_ARGB);
	pix_fmts.push_back (AV_PIX_FMT_RGBA);
	pix_fmts.push_back (AV_PIX_FMT_ABGR);
	pix_fmts.push_back (AV_PIX_FMT_BGRA);
	pix_fmts.push_back (AV_PIX_FMT_YUV420P); // 0
	pix_fmts.push_back (AV_PIX_FMT_YUV411P);
	pix_fmts.push_back (AV_PIX_FMT_YUV422P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV422P16LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10BE);
	pix_fmts.push_back (AV_PIX_FMT_UYVY422);
	pix_fmts.push_back (AV_PIX_FMT_YUVJ420P);
	pix_fmts.push_back (AV_PIX_FMT_YUVJ422P);
	pix_fmts.push_back (AV_PIX_FMT_YUVJ444P);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P10BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P10BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P10BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P16BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P16BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P16BE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA420P16LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA422P16LE);
	pix_fmts.push_back (AV_PIX_FMT_YUVA444P16LE);
	pix_fmts.push_back (AV_PIX_FMT_RGB555LE); // 46
	
	int N = 0;
	for (list<AVPixelFormat>::const_iterator i = pix_fmts.begin(); i != pix_fmts.end(); ++i) {
		boost::shared_ptr<Image> foo (new Image (*i, in_size, true));
		foo->make_black ();
		boost::shared_ptr<Image> bar = foo->scale (out_size, PIX_FMT_RGB24, true);
		
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

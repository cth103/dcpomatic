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

/* Check that Image::make_black works, and doesn't use values which crash
   sws_scale().
*/
BOOST_AUTO_TEST_CASE (make_black_test)
{
	libdcp::Size in_size (512, 512);
	libdcp::Size out_size (1024, 1024);

	list<AVPixelFormat> pix_fmts;
	pix_fmts.push_back (AV_PIX_FMT_RGB24);
	pix_fmts.push_back (AV_PIX_FMT_YUV420P);
	pix_fmts.push_back (AV_PIX_FMT_YUV422P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV422P16LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10BE);
	pix_fmts.push_back (AV_PIX_FMT_UYVY422);

	int N = 0;
	for (list<AVPixelFormat>::const_iterator i = pix_fmts.begin(); i != pix_fmts.end(); ++i) {
		boost::shared_ptr<Image> foo (new SimpleImage (*i, in_size, true));
		foo->make_black ();
		boost::shared_ptr<Image> bar = foo->scale_and_convert_to_rgb (out_size, Scaler::from_id ("bicubic"), true);
		
		uint8_t* p = bar->data()[0];
		for (int y = 0; y < bar->size().height; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < bar->line_size()[0]; ++x) {
				BOOST_CHECK_EQUAL (*q++, 0);
			}
			p += bar->stride()[0];
		}

		++N;
	}
}

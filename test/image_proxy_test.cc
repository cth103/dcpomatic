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


#include "lib/ffmpeg_image_proxy.h"
#include "lib/j2k_image_proxy.h"
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>


using boost::shared_ptr;


static const char data_file0[] = "test/data/player_seek_test_0.png";
static const char data_file1[] = "test/data/player_seek_test_1.png";


BOOST_AUTO_TEST_CASE (j2k_image_proxy_same_test)
{
	/* The files don't matter here, we just need some data to compare */

	{
		shared_ptr<J2KImageProxy> proxy1(new J2KImageProxy(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48));
		shared_ptr<J2KImageProxy> proxy2(new J2KImageProxy(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48));
		BOOST_CHECK (proxy1->same(proxy2));
	}

	{
		shared_ptr<J2KImageProxy> proxy1(new J2KImageProxy(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48));
		shared_ptr<J2KImageProxy> proxy2(new J2KImageProxy(data_file1, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48));
		BOOST_CHECK (!proxy1->same(proxy2));
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_proxy_same_test)
{
	{
		shared_ptr<FFmpegImageProxy> proxy1(new FFmpegImageProxy(data_file0, VIDEO_RANGE_FULL));
		shared_ptr<FFmpegImageProxy> proxy2(new FFmpegImageProxy(data_file0, VIDEO_RANGE_FULL));
		BOOST_CHECK (proxy1->same(proxy2));
	}

	{
		shared_ptr<FFmpegImageProxy> proxy1(new FFmpegImageProxy(data_file0, VIDEO_RANGE_FULL));
		shared_ptr<FFmpegImageProxy> proxy2(new FFmpegImageProxy(data_file1, VIDEO_RANGE_FULL));
		BOOST_CHECK (!proxy1->same(proxy2));
	}
}


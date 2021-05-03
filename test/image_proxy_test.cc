/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;


static const boost::filesystem::path data_file0 = TestPaths::private_data() / "player_seek_test_0.png";
static const boost::filesystem::path data_file1 = TestPaths::private_data() / "player_seek_test_1.png";


BOOST_AUTO_TEST_CASE (j2k_image_proxy_same_test)
{
	/* The files don't matter here, we just need some data to compare */

	{
		auto proxy1 = make_shared<J2KImageProxy>(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48);
		auto proxy2 = make_shared<J2KImageProxy>(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48);
		BOOST_CHECK (proxy1->same(proxy2));
	}

	{
		auto proxy1 = make_shared<J2KImageProxy>(data_file0, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48);
		auto proxy2 = make_shared<J2KImageProxy>(data_file1, dcp::Size(1998, 1080), AV_PIX_FMT_RGB48);
		BOOST_CHECK (!proxy1->same(proxy2));
	}
}


BOOST_AUTO_TEST_CASE (ffmpeg_image_proxy_same_test)
{
	{
		auto proxy1 = make_shared<FFmpegImageProxy>(data_file0, VideoRange::FULL);
		auto proxy2 = make_shared<FFmpegImageProxy>(data_file0, VideoRange::FULL);
		BOOST_CHECK (proxy1->same(proxy2));
	}

	{
		auto proxy1 = make_shared<FFmpegImageProxy>(data_file0, VideoRange::FULL);
		auto proxy2 = make_shared<FFmpegImageProxy>(data_file1, VideoRange::FULL);
		BOOST_CHECK (!proxy1->same(proxy2));
	}
}


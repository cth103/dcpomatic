/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/dcp_video.h"
#include "lib/image.h"
#include "lib/player_video.h"
#include "lib/raw_image_proxy.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::make_shared;


BOOST_AUTO_TEST_CASE (low_bitrate_test)
{
	auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), true);
	image->make_black ();
	auto proxy = make_shared<RawImageProxy>(image);
	auto frame = make_shared<PlayerVideo>(proxy, Crop(), boost::optional<double>(), dcp::Size(1998, 1080), dcp::Size(1998, 1080), Eyes::BOTH, Part::WHOLE, boost::optional<ColourConversion>(), VideoRange::FULL, std::weak_ptr<Content>(), boost::optional<Frame>(), false);
	auto dcp_video = make_shared<DCPVideo>(frame, 0, 24, 100000000, Resolution::TWO_K);
	auto j2k = dcp_video->encode_locally();
	BOOST_REQUIRE (j2k.size() >= 16536);
}



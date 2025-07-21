/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/content_factory.h"
#include "lib/dcpomatic_time.h"
#include "lib/image_png.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;



BOOST_AUTO_TEST_CASE(video_trim_test)
{
	auto content = content_factory("test/data/count300bd24.m2ts")[0];
	auto film = new_test_film("trim_video_test", { content });

	content->set_trim_start(film, dcpomatic::ContentTime::from_frames(8, 24));

	shared_ptr<PlayerVideo> first_video;

	auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
	player->Video.connect([&first_video](shared_ptr<PlayerVideo> video, dcpomatic::DCPTime) {
	      first_video = video;
	});

	while (!first_video) {
		BOOST_REQUIRE(!player->pass());
	}

	image_as_png(first_video->image(force(AV_PIX_FMT_RGB24), VideoRange::FULL, true)).write("build/test/video_trim_test.png");
	check_image("test/data/video_trim_test.png", "build/test/video_trim_test.png");
}


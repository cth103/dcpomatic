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
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(video_content_fade_test)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("video_content_fade_test", { content });

	content->video->set_length(240);
	content->set_trim_start(dcpomatic::ContentTime::from_frames(24, 24));
	content->video->set_fade_in(15);
	content->video->set_fade_out(4);

	/* Before fade-in */
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(24 - 12, 24)).get_value_or(-99) == 0);
	/* Start of fade-in */
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(24, 24)).get_value_or(-99) == 0);
	/* During fade-in */
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(24 + 13, 24)).get_value_or(-99) > 0);
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(24 + 13, 24)).get_value_or(-99) < 1);
	/* After fade-in */
	BOOST_CHECK(!static_cast<bool>(content->video->fade(film, dcpomatic::ContentTime::from_frames(24 + 55, 24))));
	/* During fade-out */
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(240 - 16, 24)).get_value_or(-90) <= 1);
	/* After fade-out */
	BOOST_CHECK(content->video->fade(film, dcpomatic::ContentTime::from_frames(240 + 20, 24)).get_value_or(-90) >= 0);
}


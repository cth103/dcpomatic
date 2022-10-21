/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "lib/compose.hpp"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/user_property.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::list;
using std::string;


static
void
colour_range_test(string name, boost::filesystem::path file, string ref)
{
	auto content = content_factory(file);
	BOOST_REQUIRE(!content.empty());
	auto film = new_test_film2(String::compose("ffmpeg_properties_test_%1", name), { content.front() });

	auto properties = content.front()->user_properties(film);
	auto iter = std::find_if(properties.begin(), properties.end(), [](UserProperty const& property) { return property.key == "Colour range"; });
	BOOST_REQUIRE(iter != properties.end());
	BOOST_CHECK_EQUAL(iter->value, ref);
}


BOOST_AUTO_TEST_CASE(ffmpeg_properties_test)
{
	colour_range_test("1", "test/data/test.mp4", "Unspecified");
	colour_range_test("2", TestPaths::private_data() / "arrietty_JP-EN.mkv", "Limited / video (16-235)");
	colour_range_test("3", "test/data/8bit_full_420.mp4", "Full (0-255)");
	colour_range_test("4", "test/data/8bit_full_422.mp4", "Full (0-255)");
	colour_range_test("5", "test/data/8bit_full_444.mp4", "Full (0-255)");
	colour_range_test("6", "test/data/8bit_video_420.mp4", "Limited / video (16-235)");
	colour_range_test("7", "test/data/8bit_video_422.mp4", "Limited / video (16-235)");
	colour_range_test("8", "test/data/8bit_video_444.mp4", "Limited / video (16-235)");
	colour_range_test("9", "test/data/10bit_full_420.mp4", "Full (0-1023)");
	colour_range_test("10", "test/data/10bit_full_422.mp4", "Full (0-1023)");
	colour_range_test("11", "test/data/10bit_full_444.mp4", "Full (0-1023)");
	colour_range_test("12", "test/data/10bit_video_420.mp4", "Limited / video (64-940)");
	colour_range_test("13", "test/data/10bit_video_422.mp4", "Limited / video (64-940)");
	colour_range_test("14", "test/data/10bit_video_444.mp4", "Limited / video (64-940)");
}

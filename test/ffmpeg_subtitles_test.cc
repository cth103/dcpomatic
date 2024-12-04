/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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
#include "lib/film.h"
#include "lib/player.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


BOOST_AUTO_TEST_CASE(decoding_ssa_subs_from_mkv)
{
	auto subs = content_factory(TestPaths::private_data() / "ssa_subs.mkv")[0];
	auto film = new_test_film("decoding_ssa_subs_from_mkv", { subs });
	subs->text[0]->set_use(true);

	vector<string> lines;

	auto player = make_shared<Player>(film, film->playlist());
	player->Text.connect([&lines](PlayerText text, TextType, optional<DCPTextTrack>, dcpomatic::DCPTimePeriod) {
		for (auto i: text.string) {
			lines.push_back(i.text());
		}
	});

	while (lines.size() <= 2 && !player->pass()) {}

	BOOST_REQUIRE_EQUAL(lines.size(), 3U);
	BOOST_CHECK_EQUAL(lines[0], "-You're hungry.");
	BOOST_CHECK_EQUAL(lines[1], "-Unit 14, nothing's happening");
	BOOST_CHECK_EQUAL(lines[2], "here, we're gonna go to the beach.");
}


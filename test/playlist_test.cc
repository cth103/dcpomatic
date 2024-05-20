/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/playlist.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::vector;


static
shared_ptr<Film>
setup(vector<shared_ptr<Content>>& content, vector<dcpomatic::DCPTime>& positions, vector<dcpomatic::DCPTime>& lengths)
{
	for (auto i = 0; i < 3; ++i) {
		content.push_back(content_factory("test/data/flat_red.png")[0]);
	}

	auto film = new_test_film("playlist_move_later_test", content);

	for (auto i: content) {
		positions.push_back(i->position());
	}

	for (auto i: content) {
		lengths.push_back(i->length_after_trim(film));
	}

	return film;
}


BOOST_AUTO_TEST_CASE(playlist_move_later_test1)
{
	vector<shared_ptr<Content>> content;
	vector<dcpomatic::DCPTime> positions;
	vector<dcpomatic::DCPTime> lengths;
	auto film = setup(content, positions, lengths);

	film->move_content_later(content[1]);

	auto moved_content = film->content();
	BOOST_REQUIRE_EQUAL(moved_content.size(), 3U);

	BOOST_CHECK_EQUAL(moved_content[0], content[0]);
	BOOST_CHECK_EQUAL(moved_content[1], content[2]);
	BOOST_CHECK_EQUAL(moved_content[2], content[1]);

	BOOST_CHECK(content[0]->position() == positions[0]);
	BOOST_CHECK(content[1]->position() == positions[1] + lengths[2]);
	BOOST_CHECK(content[2]->position() == positions[1]);
}


BOOST_AUTO_TEST_CASE(playlist_move_later_test2)
{
	vector<shared_ptr<Content>> content;
	vector<dcpomatic::DCPTime> positions;
	vector<dcpomatic::DCPTime> lengths;
	auto film = setup(content, positions, lengths);

	film->move_content_later(content[0]);

	auto moved_content = film->content();
	BOOST_REQUIRE_EQUAL(moved_content.size(), 3U);

	BOOST_CHECK_EQUAL(moved_content[0], content[1]);
	BOOST_CHECK_EQUAL(moved_content[1], content[0]);
	BOOST_CHECK_EQUAL(moved_content[2], content[2]);

	BOOST_CHECK(content[0]->position() == positions[0] + lengths[1]);
	BOOST_CHECK(content[1]->position() == positions[0]);
	BOOST_CHECK(content[2]->position() == positions[2]);
}


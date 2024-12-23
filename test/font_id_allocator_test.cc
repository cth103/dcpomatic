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


#include "lib/font_id_allocator.h"
#include <boost/test/unit_test.hpp>


using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE(font_id_allocator_test_without_disambiguation)
{
	FontIDAllocator allocator;

	/* Reel 0 has just one asset with two fonts */
	allocator.add_font(0, "asset1", "font");
	allocator.add_font(0, "asset1", "font2");

	/* Reel 1 has two assets each with two more fonts */
	allocator.add_font(1, "asset2", "font");
	allocator.add_font(1, "asset2", "font2");
	allocator.add_font(1, "asset3", "font3");
	allocator.add_font(1, "asset3", "font4");

	allocator.allocate();

	BOOST_CHECK_EQUAL(allocator.font_id(0, "asset1", "font"), "font");
	BOOST_CHECK_EQUAL(allocator.font_id(0, "asset1", "font2"), "font2");
	BOOST_CHECK_EQUAL(allocator.font_id(1, "asset2", "font"), "0_font");
	BOOST_CHECK_EQUAL(allocator.font_id(1, "asset2", "font2"), "0_font2");
	BOOST_CHECK_EQUAL(allocator.font_id(1, "asset3", "font3"), "font3");
	BOOST_CHECK_EQUAL(allocator.font_id(1, "asset3", "font4"), "font4");
}


BOOST_AUTO_TEST_CASE(font_id_allocator_test_with_disambiguation)
{
	FontIDAllocator allocator;

	/* Reel 0 has two assets each with a font with the same ID (perhaps a subtitle and a ccap).
	 * This would have crashed DCP-o-matic before the FontIDAllocator change (bug #2600)
	 * so it's OK if the second font gets a new index that we didn't see before.
	 */
	allocator.add_font(0, "asset1", "font");
	allocator.add_font(0, "asset2", "font");

	/* Reel 1 has one asset with another font */
	allocator.add_font(1, "asset3", "font1");

	allocator.allocate();

	BOOST_CHECK(allocator.font_id(0, "asset1", "font") == "font");
	BOOST_CHECK(allocator.font_id(0, "asset2", "font") == "0_font");
	BOOST_CHECK(allocator.font_id(1, "asset3", "font1") == "font1");
}


/* Bug #2822: multiple reels, each with subs + closed captions, and each using the same
 * basic font ID.
 */
BOOST_AUTO_TEST_CASE(font_id_allocator_test_with_disambiguation2)
{
	FontIDAllocator allocator;

	allocator.add_font(0, "asset1", "font");
	allocator.add_font(0, "asset2", "font");

	allocator.add_font(1, "asset1", "font");
	allocator.add_font(1, "asset2", "font");

	allocator.allocate();
	vector<string> ids = {
		allocator.font_id(0, "asset1", "font"),
		allocator.font_id(0, "asset2", "font"),
		allocator.font_id(1, "asset1", "font"),
		allocator.font_id(1, "asset2", "font")
	};

	std::sort(ids.begin(), ids.end());
	BOOST_CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
}

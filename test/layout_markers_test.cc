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


#include "lib/dcpomatic_time.h"
#include "lib/layout_markers.h"
#include <dcp/types.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::map;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE(allocation_row_test)
{
	AllocationRow row;
	BOOST_CHECK(row.allocate(0, 5));
	BOOST_CHECK(row.allocate(6, 10));
	BOOST_CHECK(!row.allocate(1, 3));
	BOOST_CHECK(!row.allocate(4, 7));
	BOOST_CHECK(row.allocate(19, 20));
	BOOST_CHECK(!row.allocate(19, 20));
	BOOST_CHECK(!row.allocate(17, 20));
	BOOST_CHECK(!row.allocate(10, 16));
	BOOST_CHECK(!row.allocate(11, 19));
	BOOST_CHECK(row.allocate(11, 18));
}


static
vector<string>
plot(vector<MarkerLayoutComponent> const& components)
{
	vector<string> out;

	auto write = [&out](int x, int y, char c) {
		while (y >= static_cast<int>(out.size())) {
			out.push_back({});
		}
		while (x >= static_cast<int>(out[y].length())) {
			out[y] += " ";
		}
		if (out[y][x] == ' ') {
			out[y][x] = c;
		}
	};

	for (auto component: components) {
		switch (component.type) {
		case MarkerLayoutComponent::Type::LEFT:
			write(component.x1, component.y, '/');
			break;
		case MarkerLayoutComponent::Type::RIGHT:
			write(component.x1, component.y, '|');
			break;
		case MarkerLayoutComponent::Type::LABEL:
			for (auto i = 0U; i < component.text.length(); ++i) {
				write(component.x1 + i, component.y, component.text[i]);
			}
			break;
		case MarkerLayoutComponent::Type::LINE:
			for (auto x = component.x1; x <= component.x2; ++x) {
				write(x, component.y, '-');
			}
			break;
		}
	}

	return out;
}


BOOST_AUTO_TEST_CASE(layout_test1)
{
	map<dcp::Marker, dcpomatic::DCPTime> markers = {
		{ dcp::Marker::FFOB, dcpomatic::DCPTime(1) },
		{ dcp::Marker::LFOB, dcpomatic::DCPTime(9) },
		{ dcp::Marker::FFTC, dcpomatic::DCPTime(13) },
		{ dcp::Marker::LFTC, dcpomatic::DCPTime(17) },
		{ dcp::Marker::FFOI, dcpomatic::DCPTime(12) },
		{ dcp::Marker::LFOI, dcpomatic::DCPTime(25) },
		{ dcp::Marker::FFEC, dcpomatic::DCPTime(20) },
		{ dcp::Marker::LFEC, dcpomatic::DCPTime(30) },
		{ dcp::Marker::FFMC, dcpomatic::DCPTime(0) },
		{ dcp::Marker::LFMC, dcpomatic::DCPTime(3) }
	};

	auto components = layout_markers(markers, 30, dcpomatic::DCPTime(30), 1, 1, [](std::string s) { return s.length(); });

	auto check = plot(components);
	BOOST_REQUIRE_EQUAL(check.size(), 2U);
	BOOST_CHECK_EQUAL(check[0], " /--RB---|   /TC-|  /---EC----|");
	BOOST_CHECK_EQUAL(check[1], "/C-|        /----IN------|"    );

	for (auto const& line: check) {
		std::cout << line << "\n";
	}
}


BOOST_AUTO_TEST_CASE(layout_test2)
{
	map<dcp::Marker, dcpomatic::DCPTime> markers = {
		{ dcp::Marker::FFOB, dcpomatic::DCPTime(1) },
	};

	auto components = layout_markers(markers, 4, dcpomatic::DCPTime(4), 2, 2, [](std::string s) { return s.length(); });

	auto check = plot(components);
	BOOST_REQUIRE_EQUAL(check.size(), 1U);
	BOOST_CHECK_EQUAL(check[0], " /-RB");

	for (auto const& line: check) {
		std::cout << line << "\n";
	}
}


BOOST_AUTO_TEST_CASE(layout_test3)
{
	map<dcp::Marker, dcpomatic::DCPTime> markers = {
		{ dcp::Marker::LFOB, dcpomatic::DCPTime(4) },
	};

	auto components = layout_markers(markers, 4, dcpomatic::DCPTime(4), 2, 2, [](std::string s) { return s.length(); });

	auto check = plot(components);
	BOOST_REQUIRE_EQUAL(check.size(), 1U);
	BOOST_CHECK_EQUAL(check[0], "RB--|");

	for (auto const& line: check) {
		std::cout << line << "\n";
	}
}

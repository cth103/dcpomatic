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


#include "dcpomatic_time.h"
#include <dcp/types.h>
#include <string>
#include <utility>
#include <vector>


class MarkerLayoutComponent
{
public:
	enum class Type {
		LEFT,
		RIGHT,
		LINE,
		LABEL
	};

	MarkerLayoutComponent(Type type_, int x1_, int y_, dcp::Marker marker_, dcpomatic::DCPTime t1_)
		: type(type_)
		, x1(x1_)
		, y(y_)
		, marker(marker_)
		, t1(t1_)
	{}

	MarkerLayoutComponent(Type type_, int x1_, int x2_, int y_)
		: type(type_)
		, x1(x1_)
		, x2(x2_)
		, y(y_)
	{}

	MarkerLayoutComponent(Type type_, int x1_, int width, int y_, std::string text_)
		: type(type_)
		, x1(x1_)
		, x2(x1_ + width)
		, y(y_)
		, text(text_)
	{}

	Type type = Type::LINE;
	int x1 = 0;
	int x2 = 0;
	int y = 0;
	boost::optional<dcp::Marker> marker;
	dcpomatic::DCPTime t1;
	std::string text;
};


class AllocationRow
{
public:
	AllocationRow() = default;

	/** @return true if allocation succeded, otherwise false */
	bool allocate(int x1, int x2);

private:
	std::vector<std::pair<int, int>> _allocated;
};


std::vector<MarkerLayoutComponent> layout_markers(
	std::map<dcp::Marker, dcpomatic::DCPTime> const& markers,
	int width_in_pixels,
	dcpomatic::DCPTime width_in_time,
	int label_to_end_gap,
	int outside_label_gap,
	std::function<int (std::string)> text_width
);


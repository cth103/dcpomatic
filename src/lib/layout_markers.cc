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
#include "layout_markers.h"
#include <dcp/types.h>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "i18n.h"


using std::function;
using std::map;
using std::max;
using std::min;
using std::pair;
using std::string;
using std::vector;
using namespace std::placeholders;


vector<MarkerLayoutComponent>
layout_markers(
	map<dcp::Marker, dcpomatic::DCPTime> const& markers,
	int width_in_pixels,
	dcpomatic::DCPTime width_in_time,
	int label_to_end_gap,
	int outside_label_gap,
	function<int (string)> text_width
)
{
	float const pixels_per_unit_time = static_cast<float>(width_in_pixels) / width_in_time.get();
	auto pixels_between = [&](dcpomatic::DCPTime t1, dcpomatic::DCPTime t2) {
		return pixels_per_unit_time * t2.get() - pixels_per_unit_time * t1.get();
	};

	vector<AllocationRow> allocations;

	auto allocate = [&](int x1, int x2) -> int {
		int index = 0;
		for (auto& row: allocations) {
			if (row.allocate(x1, x2)) {
				return index;
			}
			++index;
		}

		auto row = AllocationRow();
		row.allocate(x1, x2);
		allocations.push_back(row);
		return index;
	};

	vector<MarkerLayoutComponent> components;
	auto layout_pair = [&](string name, pair<dcp::Marker, dcpomatic::DCPTime> start, pair<dcp::Marker, dcpomatic::DCPTime> end) {
		auto const width = text_width(name);
		int const x1 = floor(pixels_per_unit_time * start.second.get());
		int const x2 = ceil(pixels_per_unit_time * end.second.get());

		int label_x = 0;
		int y = 0;

		if (pixels_between(start.second, end.second) <= text_width(name)) {
			if (x1 > width_in_pixels / 2) {
				label_x = x1 - outside_label_gap - width;
				y = allocate(x1 - outside_label_gap - width, x2);
			} else {
				label_x = x2 + outside_label_gap;
				y = allocate(x1, x2 + outside_label_gap + width);
			}
		} else {
			label_x = (x1 + x2 - width) / 2;
			y = allocate(x1, x2);
		}

		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LEFT, x1, y, start.first, start.second});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::RIGHT, x2, y, end.first, end.second});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LABEL, label_x, width, y, name});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LINE, x1, x2, y});
	};

	auto layout_left = [&](string name, pair<dcp::Marker, dcpomatic::DCPTime> marker) {
		int const x1 = floor(pixels_per_unit_time * marker.second.get());
		auto const width = text_width(name);
		auto const y = allocate(x1, x1 + label_to_end_gap + width);
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LEFT, x1, y, marker.first, marker.second});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LABEL, x1 + label_to_end_gap, width, y, name});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LINE, x1, x1 + label_to_end_gap, y});
	};

	auto layout_right = [&](string name, pair<dcp::Marker, dcpomatic::DCPTime> marker) {
		int const x2 = floor(pixels_per_unit_time * marker.second.get());
		auto const width = text_width(name);
		auto const y = allocate(x2 - label_to_end_gap - width, x2);
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::RIGHT, x2, y, marker.first, marker.second});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LABEL, x2 - label_to_end_gap - width, width, y, name});
		components.push_back(MarkerLayoutComponent{MarkerLayoutComponent::Type::LINE, x2 - label_to_end_gap, x2, y});
	};

	auto check_pair = [&](string name, dcp::Marker a, dcp::Marker b) {
		auto fa = markers.find(a);
		auto fb = markers.find(b);
		if (fa != markers.end() && fb != markers.end()) {
			layout_pair(name, *fa, *fb);
		} else if (fa != markers.end()) {
			layout_left(name, *fa);
		} else if (fb != markers.end()) {
			layout_right(name, *fb);
		}
	};

	check_pair(_("RB"), dcp::Marker::FFOB, dcp::Marker::LFOB);
	check_pair(_("TC"), dcp::Marker::FFTC, dcp::Marker::LFTC);
	check_pair(_("IN"), dcp::Marker::FFOI, dcp::Marker::LFOI);
	check_pair(_("EC"), dcp::Marker::FFEC, dcp::Marker::LFEC);
	check_pair(_("MC"), dcp::Marker::FFMC, dcp::Marker::LFMC);

	return components;
}


bool
AllocationRow::allocate(int x1, int x2)
{
	auto overlaps = [](pair<int, int> a, pair<int, int> b) {
		return max(a.first, b.first) <= min(a.second, b.second);
	};

	if (std::none_of(_allocated.begin(), _allocated.end(), std::bind(overlaps, pair{x1, x2}, _1))) {
		_allocated.push_back({x1, x2});
		return true;
	}

	return false;
}


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


#include "dcpomatic_assert.h"
#include "layout_closed_captions.h"
#include "string_text.h"
#include "util.h"


using std::string;
using std::vector;
using boost::optional;


vector<string>
layout_closed_captions(vector<StringText> text)
{
	auto from_top = [](StringText const & c)
	{
		switch (c.v_align()) {
		case dcp::VAlign::TOP:
			return c.v_position();
		case dcp::VAlign::CENTER:
			return c.v_position() + 0.5f;
		case dcp::VAlign::BOTTOM:
			return 1.0f - c.v_position();
		}
		DCPOMATIC_ASSERT(false);
		return 0.f;
	};

	std::sort(text.begin(), text.end(), [&](StringText const & a, StringText const & b) { return from_top(a) < from_top(b); });

	vector<string> strings;
	string current;
	optional<float> last_position;
	for (auto const& t: text) {
		if (last_position && !text_positions_close(*last_position, t.v_position()) && !current.empty()) {
			strings.push_back(current);
			current = "";
		}
		current += t.text();
		last_position = t.v_position();
	}

	if (!current.empty()) {
		strings.push_back(current);
	}

	return strings;
}



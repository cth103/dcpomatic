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


#include "dcpomatic_assert.h"


/** Comparator to allow dcpomatic::Font::Content to be compared for use in a map */
struct FontComparator
{
	bool operator()(dcpomatic::Font::Content const& a, dcpomatic::Font::Content const& b) const {
		auto const a_has_file = static_cast<bool>(a.file);
		auto const b_has_file = static_cast<bool>(b.file);
		auto const a_has_data = static_cast<bool>(a.data);
		auto const b_has_data = static_cast<bool>(b.data);

		if (!a_has_file && !a_has_data && !b_has_file && !b_has_data) {
			/* Neither font has any font data: a == b */
			return false;
		} else if (!a_has_file && !a_has_data) {
			/* Fonts with no data are the "lowest": a < b */
			return true;
		} else if (!b_has_file && !b_has_data) {
			/* ... so here b < a */
			return false;
		} else if (a_has_file && !b_has_file) {
			/* Fonts with file are lower than fonts with data: a < b */
			return true;
		} else if (!a_has_file && b_has_file) {
			/* ... so here b < a */
			return false;
		} else if (a_has_file && b_has_file) {
			/* Compared as "equals" */
			return a.file->string() < b.file->string();
		} else if (a_has_data && b_has_data) {
			/* Compared as "equals" */
			auto const a_size = a.data->size();
			auto const b_size = b.data->size();
			if (a_size != b_size) {
				return a_size < b_size;
			}
			return memcmp(a.data->data(), b.data->data(), a_size) < 0;
		}

		/* Should never get here */
		DCPOMATIC_ASSERT(false);
		return false;
	};
};



/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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
#include "resolution.h"


using std::string;


/** @param r Resolution.
 *  @return Untranslated string representation.
 */
string
resolution_to_string (Resolution r)
{
	switch (r) {
	case Resolution::TWO_K:
		return "2K";
	case Resolution::FOUR_K:
		return "4K";
	}

	DCPOMATIC_ASSERT (false);
	return "";
}


Resolution
string_to_resolution (string s)
{
	if (s == "2K") {
		return Resolution::TWO_K;
	}

	if (s == "4K") {
		return Resolution::FOUR_K;
	}

	DCPOMATIC_ASSERT (false);
	return Resolution::TWO_K;
}



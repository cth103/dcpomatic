/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "types.h"

using std::max;
using std::min;
using std::string;

bool operator== (Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}

bool operator!= (Crop const & a, Crop const & b)
{
	return !(a == b);
}

/** @param r Resolution.
 *  @return Untranslated string representation.
 */
string
resolution_to_string (Resolution r)
{
	switch (r) {
	case RESOLUTION_2K:
		return "2K";
	case RESOLUTION_4K:
		return "4K";
	}

	assert (false);
	return "";
}


Resolution
string_to_resolution (string s)
{
	if (s == "2K") {
		return RESOLUTION_2K;
	}

	if (s == "4K") {
		return RESOLUTION_4K;
	}

	assert (false);
	return RESOLUTION_2K;
}

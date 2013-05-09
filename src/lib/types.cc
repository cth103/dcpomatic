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

bool operator== (Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}

bool operator!= (Crop const & a, Crop const & b)
{
	return !(a == b);
}


/** @param other A Rect.
 *  @return The intersection of this with `other'.
 */
Rect
Rect::intersection (Rect const & other) const
{
	int const tx = max (x, other.x);
	int const ty = max (y, other.y);
	
	return Rect (
		tx, ty,
		min (x + width, other.x + other.width) - tx,
		min (y + height, other.y + other.height) - ty
		);
}

bool
Rect::contains (Position p) const
{
	return (p.x >= x && p.x <= (x + width) && p.y >= y && p.y <= (y + height));
}

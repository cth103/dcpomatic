/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_RECT_H
#define DCPOMATIC_RECT_H

#include "position.h"
#include <boost/optional.hpp>
#include <algorithm>

/* Put this inside a namespace as Apple put a Rect in the global namespace */

namespace dcpomatic
{

/** @struct Rect
 *  @brief A rectangle.
 */
template <class T>
class Rect
{
public:

	Rect ()
		: x (0)
		, y (0)
		, width (0)
		, height (0)
	{}

	Rect (Position<T> p, T w_, T h_)
		: x (p.x)
		, y (p.y)
		, width (w_)
		, height (h_)
	{}

	Rect (T x_, T y_, T w_, T h_)
		: x (x_)
		, y (y_)
		, width (w_)
		, height (h_)
	{}

	T x;
	T y;
	T width;
	T height;

	Position<T> position () const
	{
		return Position<T> (x, y);
	}

	boost::optional<Rect<T> > intersection (Rect<T> const & other) const
	{
		/* This isn't exactly the paragon of mathematical precision */

		T const tx = std::max (x, other.x);
		T const ty = std::max (y, other.y);

		Rect r (
			tx, ty,
			std::min (x + width, other.x + other.width) - tx,
			std::min (y + height, other.y + other.height) - ty
			);

		if (r.width < 0 || r.height < 0) {
			return boost::optional<Rect<T> > ();
		}

		return r;
	}

	void extend (Rect<T> const & other)
	{
		x = std::min (x, other.x);
		y = std::min (y, other.y);
		width = std::max (x + width, other.x + other.width) - x;
		height = std::max (y + height, other.y + other.height) - y;
	}

	Rect<T> extended (T amount) const {
		Rect<T> c = *this;
		c.x -= amount;
		c.y -= amount;
		c.width += amount * 2;
		c.height += amount * 2;
		return c;
	}

	bool contains (Position<T> p) const
	{
		return (p.x >= x && p.x <= (x + width) && p.y >= y && p.y <= (y + height));
	}
};

}

#endif

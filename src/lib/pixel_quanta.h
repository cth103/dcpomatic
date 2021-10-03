/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_PIXEL_QUANTA_H
#define DCPOMATIC_PIXEL_QUANTA_H


#include "warnings.h"

#include <libcxml/cxml.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS


class PixelQuanta
{
public:
	PixelQuanta ()
		: x(1)
		, y(1)
	{}

	/** @param x number of pixels that must not be split in the x direction; e.g. if x=2 scale should
	 *  only happen to multiples of 2 width, and x crop should only happen at multiples of 2 position.
	 *  @param y similar value for y / height.
	 */
	PixelQuanta (int x_, int y_)
		: x(x_)
		, y(y_)
	{}

	PixelQuanta (cxml::ConstNodePtr node);

	void as_xml (xmlpp::Element* node) const;

	int round_x (int x_) const;
	int round_y (int y_) const;

	int x;
	int y;
};


PixelQuanta max (PixelQuanta const& a, PixelQuanta const& b);

#endif


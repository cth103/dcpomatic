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


#include "pixel_quanta.h"
#include <fmt/format.h>


PixelQuanta::PixelQuanta (cxml::ConstNodePtr node)
	: x(node->number_child<int>("X"))
	, y(node->number_child<int>("Y"))
{

}


void
PixelQuanta::as_xml (xmlpp::Element* node) const
{
	cxml::add_text_child(node, "X", fmt::to_string(x));
	cxml::add_text_child(node, "Y", fmt::to_string(y));
}


int
PixelQuanta::round_x (int x_) const
{
	return x_ - (x_ % x);
}


int
PixelQuanta::round_y (int y_) const
{
	return y_ - (y_ % y);
}


dcp::Size
PixelQuanta::round (dcp::Size size) const
{
	return dcp::Size (round_x(size.width), round_y(size.height));
}


PixelQuanta
max (PixelQuanta const& a, PixelQuanta const& b)
{
	return { std::max(a.x, b.x), std::max(a.y, b.y) };
}


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


#include "crop.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS


using std::shared_ptr;
using std::string;
using dcp::raw_convert;


Crop::Crop(shared_ptr<cxml::Node> node)
{
	left = node->number_child<int>("LeftCrop");
	right = node->number_child<int>("RightCrop");
	top = node->number_child<int>("TopCrop");
	bottom = node->number_child<int>("BottomCrop");
}


void
Crop::as_xml(xmlpp::Element* element) const
{
	cxml::add_text_child(element, "LeftCrop", raw_convert<string>(left));
	cxml::add_text_child(element, "RightCrop", raw_convert<string>(right));
	cxml::add_text_child(element, "TopCrop", raw_convert<string>(top));
	cxml::add_text_child(element, "BottomCrop", raw_convert<string>(bottom));
}


bool operator==(Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}


bool operator!=(Crop const & a, Crop const & b)
{
	return !(a == b);
}


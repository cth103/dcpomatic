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
#include <libxml++/libxml++.h>


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
Crop::as_xml(xmlpp::Node* node) const
{
	node->add_child("LeftCrop")->add_child_text(raw_convert<string>(left));
	node->add_child("RightCrop")->add_child_text(raw_convert<string>(right));
	node->add_child("TopCrop")->add_child_text(raw_convert<string>(top));
	node->add_child("BottomCrop")->add_child_text(raw_convert<string>(bottom));
}


bool operator==(Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}


bool operator!=(Crop const & a, Crop const & b)
{
	return !(a == b);
}


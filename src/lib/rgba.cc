/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "rgba.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/lexical_cast.hpp>


using std::string;
using boost::lexical_cast;


RGBA::RGBA (cxml::ConstNodePtr node)
{
	r = node->number_child<int>("R");
	g = node->number_child<int>("G");
	b = node->number_child<int>("B");
	a = node->number_child<int>("A");
}


void
RGBA::as_xml(xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "R", lexical_cast<string>(int(r)));
	cxml::add_text_child(parent, "G", lexical_cast<string>(int(g)));
	cxml::add_text_child(parent, "B", lexical_cast<string>(int(b)));
	cxml::add_text_child(parent, "A", lexical_cast<string>(int(a)));
}


bool
RGBA::operator< (RGBA const & other) const
{
	if (r != other.r) {
		return r < other.r;
	}

	if (g != other.g) {
		return g < other.g;
	}

	if (b != other.b) {
		return b < other.b;
	}

	return a < other.a;
}

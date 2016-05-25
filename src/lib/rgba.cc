/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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
#include <libxml++/libxml++.h>
#include <boost/lexical_cast.hpp>

using std::string;
using boost::lexical_cast;

RGBA::RGBA (cxml::ConstNodePtr node)
{
	r = node->number_child<int> ("R");
	g = node->number_child<int> ("G");
	b = node->number_child<int> ("B");
	a = node->number_child<int> ("A");
}

void
RGBA::as_xml (xmlpp::Node* parent) const
{
	parent->add_child("R")->add_child_text (lexical_cast<string> (int (r)));
	parent->add_child("G")->add_child_text (lexical_cast<string> (int (g)));
	parent->add_child("B")->add_child_text (lexical_cast<string> (int (b)));
	parent->add_child("A")->add_child_text (lexical_cast<string> (int (a)));
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

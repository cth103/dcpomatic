/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "font.h"
#include "dcpomatic_assert.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::string;

static char const * names[] = {
	"Normal",
	"Italic",
	"Bold"
};

Font::Font (cxml::NodePtr node)
	: _id (node->string_child ("Id"))
{
	DCPOMATIC_ASSERT (FontFiles::VARIANTS == 3);

	BOOST_FOREACH (cxml::NodePtr i, node->node_children ("File")) {
		string variant = i->optional_string_attribute("Variant").get_value_or ("Normal");
		for (int j = 0; j < FontFiles::VARIANTS; ++j) {
			if (variant == names[j]) {
				_files.set (static_cast<FontFiles::Variant>(j), i->content());
			}
		}
	}
}

void
Font::as_xml (xmlpp::Node* node)
{
	DCPOMATIC_ASSERT (FontFiles::VARIANTS == 3);

	node->add_child("Id")->add_child_text (_id);
	for (int i = 0; i < FontFiles::VARIANTS; ++i) {
		if (_files.get(static_cast<FontFiles::Variant>(i))) {
			xmlpp::Element* e = node->add_child ("File");
			e->set_attribute ("Variant", names[i]);
			e->add_child_text (_files.get(static_cast<FontFiles::Variant>(i)).get().string ());
		}
	}
}


bool
operator== (Font const & a, Font const & b)
{
	if (a.id() != b.id()) {
		return false;
	}

	for (int i = 0; i < FontFiles::VARIANTS; ++i) {
		if (a.file(static_cast<FontFiles::Variant>(i)) != b.file(static_cast<FontFiles::Variant>(i))) {
			return false;
		}
	}

	return true;
}

bool
operator!= (Font const & a, Font const & b)
{
	return !(a == b);
}

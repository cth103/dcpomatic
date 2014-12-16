/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include <libxml++/libxml++.h>

Font::Font (cxml::NodePtr node)
{
	id = node->optional_string_child ("Id");
	file = node->optional_string_child ("File");
}

void
Font::as_xml (xmlpp::Node* node)
{
	if (id) {
		node->add_child("Id")->add_child_text (id.get ());
	}

	if (file) {
		node->add_child("File")->add_child_text (file.get().string ());
	}
}

bool
operator!= (Font const & a, Font const & b)
{
	return (a.id != b.id || a.file != b.file);
}

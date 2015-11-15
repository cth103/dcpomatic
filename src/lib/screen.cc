/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "screen.h"
#include <libxml++/libxml++.h>

Screen::Screen (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
{
	if (node->optional_string_child ("Certificate")) {
		recipient = dcp::Certificate (node->string_child ("Certificate"));
	} else if (node->optional_string_child ("Recipient")) {
		recipient = dcp::Certificate (node->string_child ("Recipient"));
	}
}

void
Screen::as_xml (xmlpp::Element* parent) const
{
	parent->add_child("Name")->add_child_text (name);
	if (recipient) {
		parent->add_child("Recipient")->add_child_text (recipient->certificate (true));
	}
}

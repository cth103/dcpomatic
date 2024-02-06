/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "cinema.h"
#include "screen.h"
#include "dcpomatic_assert.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>


using std::make_shared;
using std::shared_ptr;
using std::string;
using dcp::raw_convert;
using dcpomatic::Screen;


Cinema::Cinema (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
	, notes (node->optional_string_child("Notes").get_value_or(""))
{
	for (auto i: node->node_children("Email")) {
		emails.push_back (i->content ());
	}
}

/* This is necessary so that we can use shared_from_this in add_screen (which cannot be done from
   a constructor)
*/
void
Cinema::read_screens (cxml::ConstNodePtr node)
{
	for (auto i: node->node_children("Screen")) {
		add_screen (make_shared<Screen>(i));
	}
}

void
Cinema::as_xml (xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "Name", name);

	for (auto i: emails) {
		cxml::add_text_child(parent, "Email", i);
	}

	cxml::add_text_child(parent, "Notes", notes);

	for (auto i: _screens) {
		i->as_xml(cxml::add_child(parent, "Screen"));
	}
}

void
Cinema::add_screen (shared_ptr<Screen> s)
{
	s->cinema = shared_from_this ();
	_screens.push_back (s);
}

void
Cinema::remove_screen (shared_ptr<Screen> s)
{
       auto iter = std::find(_screens.begin(), _screens.end(), s);
       if (iter != _screens.end()) {
               _screens.erase(iter);
       }
}


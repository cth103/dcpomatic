/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "cinema.h"
#include "screen.h"
#include "dcpomatic_assert.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using boost::shared_ptr;

Cinema::Cinema (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
	, _utc_offset (node->optional_number_child<int>("UTCOffset").get_value_or (0))
{
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("Email")) {
		emails.push_back (i->content ());
	}
}

/* This is necessary so that we can use shared_from_this in add_screen (which cannot be done from
   a constructor)
*/
void
Cinema::read_screens (cxml::ConstNodePtr node)
{
	list<cxml::NodePtr> s = node->node_children ("Screen");
	for (list<cxml::NodePtr>::iterator i = s.begin(); i != s.end(); ++i) {
		add_screen (shared_ptr<Screen> (new Screen (*i)));
	}
}

void
Cinema::as_xml (xmlpp::Element* parent) const
{
	parent->add_child("Name")->add_child_text (name);

	BOOST_FOREACH (string i, emails) {
		parent->add_child("Email")->add_child_text (i);
	}

	parent->add_child("UTCOffset")->add_child_text (dcp::raw_convert<string> (_utc_offset));

	BOOST_FOREACH (shared_ptr<Screen> i, _screens) {
		i->as_xml (parent->add_child ("Screen"));
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
	_screens.remove (s);
}

void
Cinema::set_utc_offset (int o)
{
	DCPOMATIC_ASSERT (o >= -11 && o <= 12);
	_utc_offset = o;
}

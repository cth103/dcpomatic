/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using boost::shared_ptr;

Cinema::Cinema (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
	, notes (node->optional_string_child("Notes").get_value_or(""))
{
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("Email")) {
		emails.push_back (i->content ());
	}

	if (node->optional_number_child<int>("UTCOffset")) {
		_utc_offset_hour = node->number_child<int>("UTCOffset");
	} else {
		_utc_offset_hour = node->optional_number_child<int>("UTCOffsetHour").get_value_or (0);
	}

	_utc_offset_minute = node->optional_number_child<int>("UTCOffsetMinute").get_value_or (0);
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

	parent->add_child("Notes")->add_child_text (notes);

	parent->add_child("UTCOffsetHour")->add_child_text (dcp::raw_convert<string> (_utc_offset_hour));
	parent->add_child("UTCOffsetMinute")->add_child_text (dcp::raw_convert<string> (_utc_offset_minute));

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
Cinema::set_utc_offset_hour (int h)
{
	DCPOMATIC_ASSERT (h >= -11 && h <= 12);
	_utc_offset_hour = h;
}

void
Cinema::set_utc_offset_minute (int m)
{
	DCPOMATIC_ASSERT (m >= 0 && m <= 59);
	_utc_offset_minute = m;
}

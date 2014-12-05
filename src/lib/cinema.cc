/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>

using std::list;
using boost::shared_ptr;

Cinema::Cinema (cxml::ConstNodePtr node)
	: name (node->string_child ("Name"))
	, email (node->string_child ("Email"))
{

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
	parent->add_child("Email")->add_child_text (email);

	for (list<shared_ptr<Screen> >::const_iterator i = _screens.begin(); i != _screens.end(); ++i) {
		(*i)->as_xml (parent->add_child ("Screen"));
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

Screen::Screen (cxml::ConstNodePtr node)
{
	name = node->string_child ("Name");
	if (node->optional_string_child ("Certificate")) {
		certificate = dcp::Certificate (node->string_child ("Certificate"));
	}
}

void
Screen::as_xml (xmlpp::Element* parent) const
{
	parent->add_child("Name")->add_child_text (name);
	if (certificate) {
		parent->add_child("Certificate")->add_child_text (certificate->certificate (true));
	}
}

		

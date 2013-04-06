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

#include <boost/lexical_cast.hpp>
#include <libcxml/cxml.h>
#include "audio_mapping.h"

using std::list;
using std::cout;
using std::make_pair;
using std::pair;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::dynamic_pointer_cast;

void
AudioMapping::add (Channel c, libdcp::Channel d)
{
	_content_to_dcp.push_back (make_pair (c, d));
}

/* XXX: this is grotty */
int
AudioMapping::dcp_channels () const
{
	for (list<pair<Channel, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (((int) i->second) > 2) {
			return 6;
		}
	}

	return 2;
}

list<AudioMapping::Channel>
AudioMapping::dcp_to_content (libdcp::Channel d) const
{
	list<AudioMapping::Channel> c;
	for (list<pair<Channel, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (i->second == d) {
			c.push_back (i->first);
		}
	}

	return c;
}

list<AudioMapping::Channel>
AudioMapping::content_channels () const
{
	list<AudioMapping::Channel> c;
	for (list<pair<Channel, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (find (c.begin(), c.end(), i->first) == c.end ()) {
			c.push_back (i->first);
		}
	}

	return c;
}

list<libdcp::Channel>
AudioMapping::content_to_dcp (Channel c) const
{
	list<libdcp::Channel> d;
	for (list<pair<Channel, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (i->first == c) {
			d.push_back (i->second);
		}
	}

	return d;
}

void
AudioMapping::as_xml (xmlpp::Node* node) const
{
	for (list<pair<Channel, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		xmlpp::Node* t = node->add_child ("Map");
		shared_ptr<const AudioContent> c = i->first.content.lock ();
		t->add_child ("Content")->add_child_text (c->file().string ());
		t->add_child ("ContentIndex")->add_child_text (lexical_cast<string> (i->first.index));
		t->add_child ("DCP")->add_child_text (lexical_cast<string> (i->second));
	}
}

void
AudioMapping::set_from_xml (ContentList const & content, shared_ptr<const cxml::Node> node)
{
	list<shared_ptr<cxml::Node> > const c = node->node_children ("Map");
	for (list<shared_ptr<cxml::Node> >::const_iterator i = c.begin(); i != c.end(); ++i) {
		string const c = (*i)->string_child ("Content");
		ContentList::const_iterator j = content.begin ();
		while (j != content.end() && (*j)->file().string() != c) {
			++j;
		}

		if (j == content.end ()) {
			continue;
		}

		shared_ptr<const AudioContent> ac = dynamic_pointer_cast<AudioContent> (*j);
		assert (ac);

		add (AudioMapping::Channel (ac, (*i)->number_child<int> ("ContentIndex")), static_cast<libdcp::Channel> ((*i)->number_child<int> ("DCP")));
	}
}

bool
operator== (AudioMapping::Channel const & a, AudioMapping::Channel const & b)
{
	shared_ptr<const AudioContent> sa = a.content.lock ();
	shared_ptr<const AudioContent> sb = b.content.lock ();
	return sa == sb && a.index == b.index;
}

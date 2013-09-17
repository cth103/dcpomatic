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
#include <libxml++/libxml++.h>
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

AudioMapping::AudioMapping ()
	: _content_channels (0)
{

}

/** Create a default AudioMapping for a given channel count.
 *  @param c Number of channels.
 */
AudioMapping::AudioMapping (int c)
	: _content_channels (c)
{

}

void
AudioMapping::make_default ()
{
	if (_content_channels == 1) {
		/* Mono -> Centre */
		add (0, libdcp::CENTRE);
	} else {
		/* 1:1 mapping */
		for (int i = 0; i < _content_channels; ++i) {
			add (i, static_cast<libdcp::Channel> (i));
		}
	}
}

AudioMapping::AudioMapping (shared_ptr<const cxml::Node> node)
{
	_content_channels = node->number_child<int> ("ContentChannels");
	
	list<shared_ptr<cxml::Node> > const c = node->node_children ("Map");
	for (list<shared_ptr<cxml::Node> >::const_iterator i = c.begin(); i != c.end(); ++i) {
		add ((*i)->number_child<int> ("ContentIndex"), static_cast<libdcp::Channel> ((*i)->number_child<int> ("DCP")));
	}
}

void
AudioMapping::add (int c, libdcp::Channel d)
{
	_content_to_dcp.push_back (make_pair (c, d));
}

list<int>
AudioMapping::dcp_to_content (libdcp::Channel d) const
{
	list<int> c;
	for (list<pair<int, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (i->second == d) {
			c.push_back (i->first);
		}
	}

	return c;
}

list<libdcp::Channel>
AudioMapping::content_to_dcp (int c) const
{
	assert (c < _content_channels);
	
	list<libdcp::Channel> d;
	for (list<pair<int, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		if (i->first == c) {
			d.push_back (i->second);
		}
	}

	return d;
}

void
AudioMapping::as_xml (xmlpp::Node* node) const
{
	node->add_child ("ContentChannels")->add_child_text (lexical_cast<string> (_content_channels));
	
	for (list<pair<int, libdcp::Channel> >::const_iterator i = _content_to_dcp.begin(); i != _content_to_dcp.end(); ++i) {
		xmlpp::Node* t = node->add_child ("Map");
		t->add_child ("ContentIndex")->add_child_text (lexical_cast<string> (i->first));
		t->add_child ("DCP")->add_child_text (lexical_cast<string> (i->second));
	}
}

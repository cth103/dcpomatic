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

#include "audio_mapping.h"

using std::list;
using std::cout;
using std::make_pair;
using std::pair;
using boost::shared_ptr;

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

bool
operator== (AudioMapping::Channel const & a, AudioMapping::Channel const & b)
{
	shared_ptr<const AudioContent> sa = a.content.lock ();
	shared_ptr<const AudioContent> sb = b.content.lock ();
	return sa == sb && a.index == b.index;
}

	
	

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

#ifndef DCPOMATIC_AUDIO_MAPPING_H
#define DCPOMATIC_AUDIO_MAPPING_H

#include <list>
#include <string>
#include <libdcp/types.h>
#include <boost/shared_ptr.hpp>
#include "audio_content.h"

class AudioMapping
{
public:
	void as_xml (xmlpp::Node *) const;
	void set_from_xml (ContentList const &, boost::shared_ptr<const cxml::Node>);
	
	struct Channel {
		Channel (boost::weak_ptr<const AudioContent> c, int i)
			: content (c)
			, index (i)
		{}
		
		boost::weak_ptr<const AudioContent> content;
		int index;
	};

	void add (Channel, libdcp::Channel);

	int dcp_channels () const;
	std::list<Channel> dcp_to_content (libdcp::Channel) const;
	std::list<std::pair<Channel, libdcp::Channel> > content_to_dcp () const {
		return _content_to_dcp;
	}

	std::list<Channel> content_channels () const;
	std::list<libdcp::Channel> content_to_dcp (Channel) const;

private:
	std::list<std::pair<Channel, libdcp::Channel> > _content_to_dcp;
};

extern bool operator== (AudioMapping::Channel const &, AudioMapping::Channel const &);

#endif

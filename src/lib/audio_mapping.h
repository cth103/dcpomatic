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

/** @file  src/lib/audio_mapping.h
 *  @brief AudioMapping class.
 */

#ifndef DCPOMATIC_AUDIO_MAPPING_H
#define DCPOMATIC_AUDIO_MAPPING_H

#include <libcxml/cxml.h>
#include <dcp/types.h>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace xmlpp {
	class Node;
}

namespace cxml {
	class Node;
}

/** @class AudioMapping.
 *  @brief A many-to-many mapping from some content channels to DCP channels.
 *
 *  The number of content channels is set on construction and fixed,
 *  and then each of those content channels are mapped to each DCP channel
 *  by a linear gain.
 */
class AudioMapping
{
public:
	AudioMapping ();
	AudioMapping (int channels);
	AudioMapping (cxml::ConstNodePtr, int);

	/* Default copy constructor is fine */
	
	void as_xml (xmlpp::Node *) const;

	void make_default ();

	void set (int, dcp::Channel, float);
	float get (int, dcp::Channel) const;

	int content_channels () const {
		return _content_channels;
	}

	std::string digest () const;

	std::list<dcp::Channel> mapped_dcp_channels () const;
	void unmap_all ();
	
private:
	void setup (int);
	
	int _content_channels;
	std::vector<std::vector<float> > _gain;
};

#endif

/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/audio_mapping.h
 *  @brief AudioMapping class.
 */

#ifndef DCPOMATIC_AUDIO_MAPPING_H
#define DCPOMATIC_AUDIO_MAPPING_H

#include <libcxml/cxml.h>
#include <vector>

namespace xmlpp {
	class Node;
}

/** @class AudioMapping.
 *  @brief A many-to-many mapping of audio channels.
 */
class AudioMapping
{
public:
	AudioMapping ();
	AudioMapping (int input_channels, int output_channels);
	AudioMapping (cxml::ConstNodePtr, int);

	/* Default copy constructor is fine */

	void as_xml (xmlpp::Node *) const;

	void make_zero ();

	void set (int input_channel, int output_channel, float);
	float get (int input_channel, int output_channel) const;

	int input_channels () const {
		return _input_channels;
	}

	int output_channels () const {
		return _output_channels;
	}

	std::string digest () const;

	std::list<int> mapped_output_channels () const;
	void unmap_all ();

private:
	void setup (int input_channels, int output_channels);

	int _input_channels;
	int _output_channels;
	std::vector<std::vector<float> > _gain;
};

#endif

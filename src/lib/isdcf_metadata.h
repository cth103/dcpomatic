/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_ISDCF_METADATA_H
#define DCPOMATIC_ISDCF_METADATA_H

#include <libcxml/cxml.h>
#include <string>

namespace xmlpp {
	class Node;
}

class ISDCFMetadata
{
public:
	ISDCFMetadata ()
		: content_version (1)
		, temp_version (false)
		, pre_release (false)
		, red_band (false)
		, two_d_version_of_three_d (false)
	{}

	explicit ISDCFMetadata (cxml::ConstNodePtr);

	void as_xml (xmlpp::Node *) const;
	void read_old_metadata (std::string, std::string);

	int content_version;
	std::string audio_language;
	std::string territory;
	std::string rating;
	std::string studio;
	std::string facility;
	/** true if this is a temporary version (without final picture or sound) */
	bool temp_version;
	/** true if this is a pre-release version (final picture and sound, but without accessibility features) */
	bool pre_release;
	/** true if this has adult content */
	bool red_band;
	/** specific theatre chain or event */
	std::string chain;
	/** true if this is a 2D version of content that also exists in 3D */
	bool two_d_version_of_three_d;
	/** mastered luminance if there are multiple versions distributed (e.g. 35, 4fl, 6fl etc.) */
	std::string mastered_luminance;
};

bool operator== (ISDCFMetadata const & a, ISDCFMetadata const & b);

#endif

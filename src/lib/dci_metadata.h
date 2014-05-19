/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DCI_METADATA_H
#define DCPOMATIC_DCI_METADATA_H

#include <string>
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>

class DCIMetadata
{
public:
	DCIMetadata ()
		: content_version (1)
	{}
	
	DCIMetadata (cxml::ConstNodePtr);

	void as_xml (xmlpp::Node *) const;
	void read_old_metadata (std::string, std::string);

	int content_version;
	std::string audio_language;
	std::string subtitle_language;
	std::string territory;
	std::string rating;
	std::string studio;
	std::string facility;
	std::string package_type;
};

#endif

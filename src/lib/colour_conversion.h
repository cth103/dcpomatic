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

#ifndef DCPOMATIC_COLOUR_CONVERSION_H
#define DCPOMATIC_COLOUR_CONVERSION_H

/* Hack for OS X compile failure; see https://bugs.launchpad.net/hugin/+bug/910160 */
#ifdef check
#undef check
#endif

#include <dcp/colour_conversion.h>
#include <libcxml/cxml.h>
#include <boost/utility.hpp>

namespace xmlpp {
	class Node;
}

class ColourConversion : public dcp::ColourConversion
{
public:
	ColourConversion ();
	ColourConversion (dcp::ColourConversion);
	ColourConversion (cxml::NodePtr, int version);

	virtual void as_xml (xmlpp::Node *) const;
	std::string identifier () const;

	boost::optional<size_t> preset () const;

	static boost::optional<ColourConversion> from_xml (cxml::NodePtr, int version);
};

class PresetColourConversion
{
public:
	PresetColourConversion ();
	PresetColourConversion (std::string, dcp::ColourConversion);
	PresetColourConversion (cxml::NodePtr node, int version);

	void as_xml (xmlpp::Node *) const;

	std::string name;
	ColourConversion conversion;
};

bool operator== (ColourConversion const &, ColourConversion const &);
bool operator!= (ColourConversion const &, ColourConversion const &);

#endif

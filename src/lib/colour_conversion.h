/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_COLOUR_CONVERSION_H
#define DCPOMATIC_COLOUR_CONVERSION_H

/* Hack for OS X compile failure; see https://bugs.launchpad.net/hugin/+bug/910160 */
#ifdef check
#undef check
#endif

#include <dcp/colour_conversion.h>
#include <libcxml/cxml.h>

namespace xmlpp {
	class Node;
}

class ColourConversion : public dcp::ColourConversion
{
public:
	ColourConversion();
	explicit ColourConversion(dcp::ColourConversion);
	ColourConversion(cxml::NodePtr, int version);
	virtual ~ColourConversion() {}

	virtual void as_xml(xmlpp::Element*) const;
	std::string identifier() const;

	boost::optional<size_t> preset() const;

	static boost::optional<ColourConversion> from_xml(cxml::NodePtr, int version);
};

class PresetColourConversion
{
public:
	PresetColourConversion();
	PresetColourConversion(std::string n, std::string i, dcp::ColourConversion);
	PresetColourConversion(cxml::NodePtr node, int version);

	ColourConversion conversion;
	std::string name;
	/** an internal short (non-internationalised) name for this preset */
	std::string id;

	static std::vector<PresetColourConversion> all() {
		return _presets;
	}

	static PresetColourConversion from_id(std::string id);

	static void setup_colour_conversion_presets();

private:
	static std::vector<PresetColourConversion> _presets;
};

bool operator==(ColourConversion const &, ColourConversion const &);
bool operator!=(ColourConversion const &, ColourConversion const &);
bool operator==(PresetColourConversion const &, PresetColourConversion const &);

#endif

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

#include <libcxml/cxml.h>
#include <boost/utility.hpp>
#include <boost/optional.hpp>
#include <boost/numeric/ublas/matrix.hpp>

namespace xmlpp {
	class Node;
}

class ColourConversion
{
public:
	ColourConversion ();
	ColourConversion (double, bool, double const matrix[3][3], double);
	ColourConversion (cxml::NodePtr);

	virtual void as_xml (xmlpp::Node *) const;
	std::string identifier () const;

	boost::optional<size_t> preset () const;

	static boost::optional<ColourConversion> from_xml (cxml::NodePtr);

	double input_gamma;
	bool input_gamma_linearised;
	boost::numeric::ublas::matrix<double> matrix;
	double output_gamma;
};

class PresetColourConversion
{
public:
	PresetColourConversion ();
	PresetColourConversion (std::string, double, bool, double const matrix[3][3], double);
	PresetColourConversion (cxml::NodePtr);

	void as_xml (xmlpp::Node *) const;

	std::string name;
	ColourConversion conversion;
};

bool operator== (ColourConversion const &, ColourConversion const &);
bool operator!= (ColourConversion const &, ColourConversion const &);

#endif

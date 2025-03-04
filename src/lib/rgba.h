/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_RGBA_H
#define DCPOMATIC_RGBA_H


#include <libcxml/cxml.h>
#include <stdint.h>


/** @class RGBA
 *  @brief A 32-bit RGBA colour.
 */
class RGBA
{
public:
	RGBA () {}

	RGBA (uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_)
		: r (r_)
		, g (g_)
		, b (b_)
		, a (a_)
	{}

	explicit RGBA (cxml::ConstNodePtr node);

	void as_xml(xmlpp::Element* parent) const;

	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;

	bool operator< (RGBA const & other) const;
};


#endif

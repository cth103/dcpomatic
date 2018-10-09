/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_EDID_H
#define DCPOMATIC_EDID_H

#include <libcxml/cxml.h>
#include <vector>
#include <string>

namespace xmlpp {
	class Node;
}

class Monitor
{
public:
	Monitor ();
	Monitor (cxml::ConstNodePtr node);
	void as_xml (xmlpp::Node* parent) const;

	std::string manufacturer_id;
	uint16_t manufacturer_product_code;
	uint32_t serial_number;
	uint8_t week_of_manufacture;
	uint8_t year_of_manufacture;
};

bool operator== (Monitor const & a, Monitor const & b);

extern std::vector<Monitor> get_monitors();

#endif

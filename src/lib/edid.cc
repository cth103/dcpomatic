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

#include "compose.hpp"
#include "edid.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

#define EDID_SYS_PATH "/sys/class/drm"
static uint8_t const edid_header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

using std::vector;
using std::string;
using std::cout;

vector<Monitor>
get_monitors()
{
	using namespace boost::filesystem;
	using namespace boost::algorithm;

	vector<Monitor> monitors;

	int card = 0;
	while (true) {
		path card_dir = String::compose("%1/card%2", EDID_SYS_PATH, card);
		if (!is_directory(card_dir)) {
			break;
		}

		for (directory_iterator i = directory_iterator(card_dir); i != directory_iterator(); ++i) {
			if (!starts_with(i->path().filename().string(), String::compose("card%1", card))) {
				continue;
			}

			FILE* edid_file = fopen(path(i->path() / "edid").string().c_str(), "r");
			if (!edid_file) {
				continue;
			}

			uint8_t edid[128];
			int const N = fread(edid, 1, sizeof(edid), edid_file);
			fclose(edid_file);
			if (N != 128) {
				continue;
			}

			if (memcmp(edid, edid_header, 8) != 0) {
				continue;
			}

			Monitor mon;

			uint16_t mid = (edid[8] << 8) | edid[9];
			mon.manufacturer_id += char(((mid >> 10) & 0x1f) + 'A' - 1);
			mon.manufacturer_id += char(((mid >> 5) & 0x1f) + 'A' - 1);
			mon.manufacturer_id += char(((mid >> 0) & 0x1f) + 'A' - 1);

			mon.manufacturer_product_code = (edid[11] << 8) | edid[10];

			mon.serial_number = (edid[15] << 24) | (edid[14] << 16) | (edid[13] << 8) | edid[12];
			mon.week_of_manufacture = edid[16];
			mon.year_of_manufacture = edid[17];
			monitors.push_back (mon);
		}

		++card;
	}

	return monitors;
}

Monitor::Monitor ()
	: manufacturer_product_code (0)
	, serial_number (0)
	, week_of_manufacture (0)
	, year_of_manufacture (0)
{

}

Monitor::Monitor (cxml::ConstNodePtr node)
	: manufacturer_id(node->string_child("ManufacturerId"))
	, manufacturer_product_code(node->number_child<uint32_t>("ManufacturerProductCode"))
	, serial_number(node->number_child<uint32_t>("SerialNumber"))
	, week_of_manufacture(node->number_child<int>("WeekOfManufacture"))
	, year_of_manufacture(node->number_child<int>("YearOfManufacture"))

{

}

void
Monitor::as_xml (xmlpp::Node* parent) const
{
	parent->add_child("ManufacturerId")->add_child_text(manufacturer_id);
	parent->add_child("ManufacturerProductCode")->add_child_text(dcp::raw_convert<string>(manufacturer_product_code));
	parent->add_child("SerialNumber")->add_child_text(dcp::raw_convert<string>(serial_number));
	parent->add_child("WeekOfManufacture")->add_child_text(dcp::raw_convert<string>(week_of_manufacture));
	parent->add_child("YearOfManufacture")->add_child_text(dcp::raw_convert<string>(year_of_manufacture));
}

bool
operator== (Monitor const & a, Monitor const & b)
{
	return a.manufacturer_id == b.manufacturer_id &&
		a.manufacturer_product_code == b.manufacturer_product_code &&
		a.serial_number == b.serial_number &&
		a.week_of_manufacture == b.week_of_manufacture &&
		a.year_of_manufacture == b.year_of_manufacture;
}

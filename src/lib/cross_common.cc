/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <iostream>

#include "i18n.h"


using std::map;
using std::string;
using std::vector;
using boost::optional;


Drive::Drive(string xml)
{
	cxml::Document doc;
	doc.read_string(xml);
	_device = doc.string_child("Device");
#ifdef DCPOMATIC_OSX
	_mounted = doc.bool_child("Mounted");
#else
	for (auto i: doc.node_children("MountPoint")) {
		_mount_points.push_back(i->content());
	}
#endif
	_size = doc.number_child<uint64_t>("Size");
	_vendor = doc.optional_string_child("Vendor");
	_model = doc.optional_string_child("Model");
}


string
Drive::as_xml() const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node("Drive");
	cxml::add_text_child(root, "Device", _device);
#ifdef DCPOMATIC_OSX
	cxml::add_text_child(root, "Mounted", _mounted ? "1" : "0");
#else
	for (auto i: _mount_points) {
		cxml::add_text_child(root, "MountPoint", i.string());
	}
#endif
	cxml::add_text_child(root, "Size", fmt::to_string(_size));
	if (_vendor) {
		cxml::add_text_child(root, "Vendor", *_vendor);
	}
	if (_model) {
		cxml::add_text_child(root, "Model", *_model);
	}

	return doc.write_to_string("UTF-8");
}


string
Drive::description() const
{
	char gb[64];
	snprintf(gb, 64, "%.1f", _size / 1000000000.0);

	string name;
	if (_vendor) {
		name += *_vendor;
	}
	if (_model) {
		if (!name.empty()) {
			name += " " + *_model;
		} else {
			name = *_model;
		}
	}
	if (name.empty()) {
		name = _("Unknown");
	}

	return fmt::format(_("{} ({} GB) [{}]"), name, gb, _device);
}


string
Drive::log_summary() const
{
	string mp;
#ifdef DCPOMATIC_OSX
	if (_mounted) {
		mp += "mounted ";
	} else {
		mp += "not mounted ";
	}
#else
	mp = "mounted on ";
	for (auto i: _mount_points) {
		mp += i.string() + ",";
	}
	if (_mount_points.empty()) {
		mp = "[none]";
	} else {
		mp = mp.substr(0, mp.length() - 1);
	}
#endif

	return fmt::format(
		"Device {} mounted on {} size {} vendor {} model {}",
		_device, mp, _size, _vendor.get_value_or("[none]"), _model.get_value_or("[none]")
			);
}


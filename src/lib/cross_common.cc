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
#include "dcpomatic_log.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/algorithm/string.hpp>
#include <iostream>

#include "i18n.h"


using std::map;
using std::string;
using std::vector;
using boost::optional;


Drive::Drive (string xml)
{
	cxml::Document doc;
	doc.read_string (xml);
	_device = doc.string_child("Device");
	for (auto i: doc.node_children("MountPoint")) {
		_mount_points.push_back (i->content());
	}
	_size = doc.number_child<uint64_t>("Size");
	_vendor = doc.optional_string_child("Vendor");
	_model = doc.optional_string_child("Model");
}


string
Drive::as_xml () const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Drive");
	root->add_child("Device")->add_child_text(_device);
	for (auto i: _mount_points) {
		root->add_child("MountPoint")->add_child_text(i.string());
	}
	root->add_child("Size")->add_child_text(dcp::raw_convert<string>(_size));
	if (_vendor) {
		root->add_child("Vendor")->add_child_text(*_vendor);
	}
	if (_model) {
		root->add_child("Model")->add_child_text(*_model);
	}

	return doc.write_to_string("UTF-8");
}


string
Drive::description () const
{
	char gb[64];
	snprintf(gb, 64, "%.1f", _size / 1000000000.0);

	string name;
	if (_vendor) {
		name += *_vendor;
	}
	if (_model) {
		if (name.size() > 0) {
			name += " " + *_model;
		} else {
			name = *_model;
		}
	}
	if (name.size() == 0) {
		name = _("Unknown");
	}

	return String::compose(_("%1 (%2 GB) [%3]"), name, gb, _device);
}


string
Drive::log_summary () const
{
	string mp;
	for (auto i: _mount_points) {
		mp += i.string() + ",";
	}
	if (mp.empty()) {
		mp = "[none]";
	} else {
		mp = mp.substr (0, mp.length() - 1);
	}

	return String::compose(
		"Device %1 mounted on %2 size %3 vendor %4 model %5",
		_device, mp, _size, _vendor.get_value_or("[none]"), _model.get_value_or("[none]")
			);
}



/* This is in _common so we can use it in unit tests */
optional<OSXMediaPath>
analyse_osx_media_path (string path)
{
	using namespace boost::algorithm;

	if (path.find("/IOHDIXController") != string::npos) {
		/* This is a disk image, so we completely ignore it */
		LOG_DISK_NC("Ignoring this as it seems to be a disk image");
		return {};
	}

	OSXMediaPath mp;
	if (starts_with(path, "IODeviceTree:")) {
		mp.real = true;
	} else if (starts_with(path, "IOService:")) {
		mp.real = false;
	} else {
		return {};
	}

	vector<string> bits;
	split(bits, path, boost::is_any_of("/"));
	for (auto i: bits) {
		if (starts_with(i, "PRT")) {
			mp.prt = i;
		}
	}

	return mp;
}


/* This is in _common so we can use it in unit tests */
vector<Drive>
osx_disks_to_drives (vector<OSXDisk> disks)
{
	using namespace boost::algorithm;

	/* Mark disks containing mounted partitions as themselves mounted */
	for (auto& i: disks) {
		if (!i.whole) {
			continue;
		}
		for (auto& j: disks) {
			if (!j.mount_points.empty() && starts_with(j.mount_point, i.mount_point)) {
				LOG_DISK("Marking %1 as mounted because %2 is", i.mount_point, j.mount_point);
				std::copy(j.mount_points.begin(), j.mount_points.end(), back_inserter(i.mount_points));
			}
		}
	}

	/* Make a map of the PRT codes and mount points of mounted, synthesized disks */
	map<string, vector<boost::filesystem::path>> mounted_synths;
	for (auto const& i: disks) {
		if (!i.real && !i.mount_points.empty()) {
			LOG_DISK("Found a mounted synth %1 with %2", i.mount_point, i.prt);
			mounted_synths[i.prt] = i.mount_points;
		}
	}

	/* Mark containers of those mounted synths as themselves mounted */
	for (auto& i: disks) {
		if (i.real) {
			auto j = mounted_synths.find(i.prt);
			if (j != mounted_synths.end()) {
				LOG_DISK("Marking %1 (%2) as mounted because it contains a mounted synth", i.mount_point, i.prt);
				std::copy(j->second.begin(), j->second.end(), back_inserter(i.mount_points));
			}
		}
	}

	vector<Drive> drives;
	for (auto const& i: disks) {
		if (i.whole) {
			/* A whole disk that is not a container for a mounted synth */
			drives.push_back(Drive(i.mount_point, i.mount_points, i.size, i.vendor, i.model));
			LOG_DISK_NC(drives.back().log_summary());
		}
	}

	return drives;
}

/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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
#include "state.h"
#include "util.h"
#include <dcp/filesystem.h>


using std::string;
using boost::optional;


boost::optional<boost::filesystem::path> State::override_path;


/* List of config versions to look for in descending order of preference;
 * i.e. look at the first one, and if that doesn't exist, try the second, etc.
 */
static std::vector<std::string> config_versions = { "2.20", "2.18", "2.16" };


static
boost::filesystem::path
config_path_or_override (optional<string> version)
{
	if (State::override_path) {
		auto p = *State::override_path;
		if (version) {
			p /= *version;
		}
		return p;
	}

	return config_path (version);
}


/** @param file State filename
 *  @return Full path to read @file from.
 */
boost::filesystem::path
State::read_path (string file)
{
	for (auto i: config_versions) {
		auto full = config_path_or_override(i) / file;
		if (dcp::filesystem::exists(full)) {
			return full;
		}
	}

	return config_path_or_override({}) / file;
}


/** @param file State filename
 *  @return Full path to write @file to.
 */
boost::filesystem::path
State::write_path (string file)
{
	auto p = config_path_or_override(config_versions.front());
	dcp::filesystem::create_directories(p);
	p /= file;
	return p;
}


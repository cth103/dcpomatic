/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "timer.h"
#include "types.h"
#include "dcpomatic_assert.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/filesystem.h>
#include <dcp/reel_asset.h>
#include <dcp/reel_file_asset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <libcxml/cxml.h>

#include "i18n.h"

using std::max;
using std::min;
using std::string;
using std::list;
using std::shared_ptr;
using std::vector;


ReelType
string_to_reel_type(string type)
{
	if (type == "single") {
		return ReelType::SINGLE;
	} else if (type == "by-video-content") {
		return ReelType::BY_VIDEO_CONTENT;
	} else if (type == "by-length") {
		return ReelType::BY_LENGTH;
	} else if (type == "custom") {
		return ReelType::CUSTOM;
	}

	DCPOMATIC_ASSERT(false);
	return ReelType::SINGLE;
}


string
reel_type_to_string(ReelType type)
{
	switch (type) {
	case ReelType::SINGLE:
		return "single";
	case ReelType::BY_VIDEO_CONTENT:
		return "by-video-content";
	case ReelType::BY_LENGTH:
		return "by-length";
	case ReelType::CUSTOM:
		return "custom";
	}

	DCPOMATIC_ASSERT(false);
	return "single";
}

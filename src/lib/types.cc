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

#include "types.h"
#include "dcpomatic_assert.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>

using std::max;
using std::min;
using std::string;
using boost::shared_ptr;

bool operator== (Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}

bool operator!= (Crop const & a, Crop const & b)
{
	return !(a == b);
}

/** @param r Resolution.
 *  @return Untranslated string representation.
 */
string
resolution_to_string (Resolution r)
{
	switch (r) {
	case RESOLUTION_2K:
		return "2K";
	case RESOLUTION_4K:
		return "4K";
	}

	DCPOMATIC_ASSERT (false);
	return "";
}


Resolution
string_to_resolution (string s)
{
	if (s == "2K") {
		return RESOLUTION_2K;
	}

	if (s == "4K") {
		return RESOLUTION_4K;
	}

	DCPOMATIC_ASSERT (false);
	return RESOLUTION_2K;
}

Crop::Crop (shared_ptr<cxml::Node> node)
{
	left = node->number_child<int> ("LeftCrop");
	right = node->number_child<int> ("RightCrop");
	top = node->number_child<int> ("TopCrop");
	bottom = node->number_child<int> ("BottomCrop");
}

void
Crop::as_xml (xmlpp::Node* node) const
{
	node->add_child("LeftCrop")->add_child_text (raw_convert<string> (left));
	node->add_child("RightCrop")->add_child_text (raw_convert<string> (right));
	node->add_child("TopCrop")->add_child_text (raw_convert<string> (top));
	node->add_child("BottomCrop")->add_child_text (raw_convert<string> (bottom));
}

string
video_frame_type_to_string (VideoFrameType t)
{
	switch (t) {
	case VIDEO_FRAME_TYPE_2D:
		return "2d";
	case VIDEO_FRAME_TYPE_3D_LEFT_RIGHT:
		return "3d-left-right";
	case VIDEO_FRAME_TYPE_3D_TOP_BOTTOM:
		return "3d-top-bottom";
	case VIDEO_FRAME_TYPE_3D_ALTERNATE:
		return "3d-alternate";
	case VIDEO_FRAME_TYPE_3D_LEFT:
		return "3d-left";
	case VIDEO_FRAME_TYPE_3D_RIGHT:
		return "3d-right";
	default:
		DCPOMATIC_ASSERT (false);
	}

	DCPOMATIC_ASSERT (false);
}

VideoFrameType
string_to_video_frame_type (string s)
{
	if (s == "2d") {
		return VIDEO_FRAME_TYPE_2D;
	} else if (s == "3d-left-right") {
		return VIDEO_FRAME_TYPE_3D_LEFT_RIGHT;
	} else if (s == "3d-top-bottom") {
		return VIDEO_FRAME_TYPE_3D_TOP_BOTTOM;
	} else if (s == "3d-alternate") {
		return VIDEO_FRAME_TYPE_3D_ALTERNATE;
	} else if (s == "3d-left") {
		return VIDEO_FRAME_TYPE_3D_LEFT;
	} else if (s == "3d-right") {
		return VIDEO_FRAME_TYPE_3D_RIGHT;
	}

	DCPOMATIC_ASSERT (false);
}

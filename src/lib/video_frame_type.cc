/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "video_frame_type.h"


using std::string;


string
video_frame_type_to_string(VideoFrameType t)
{
	switch (t) {
	case VideoFrameType::TWO_D:
		return "2d";
	case VideoFrameType::THREE_D:
		return "3d";
	case VideoFrameType::THREE_D_LEFT_RIGHT:
		return "3d-left-right";
	case VideoFrameType::THREE_D_TOP_BOTTOM:
		return "3d-top-bottom";
	case VideoFrameType::THREE_D_ALTERNATE:
		return "3d-alternate";
	case VideoFrameType::THREE_D_LEFT:
		return "3d-left";
	case VideoFrameType::THREE_D_RIGHT:
		return "3d-right";
	default:
		DCPOMATIC_ASSERT (false);
	}

	DCPOMATIC_ASSERT (false);
}

VideoFrameType
string_to_video_frame_type(string s)
{
	if (s == "2d") {
		return VideoFrameType::TWO_D;
	} else if (s == "3d") {
		return VideoFrameType::THREE_D;
	} else if (s == "3d-left-right") {
		return VideoFrameType::THREE_D_LEFT_RIGHT;
	} else if (s == "3d-top-bottom") {
		return VideoFrameType::THREE_D_TOP_BOTTOM;
	} else if (s == "3d-alternate") {
		return VideoFrameType::THREE_D_ALTERNATE;
	} else if (s == "3d-left") {
		return VideoFrameType::THREE_D_LEFT;
	} else if (s == "3d-right") {
		return VideoFrameType::THREE_D_RIGHT;
	}

	DCPOMATIC_ASSERT(false);
}


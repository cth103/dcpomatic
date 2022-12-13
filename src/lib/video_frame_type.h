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


#ifndef DCPOMATIC_VIDEO_FRAME_TYPE_H
#define DCPOMATIC_VIDEO_FRAME_TYPE_H


#include <string>


enum class VideoFrameType
{
	TWO_D,
	/** `True' 3D content, e.g. 3D DCPs */
	THREE_D,
	THREE_D_LEFT_RIGHT,
	THREE_D_TOP_BOTTOM,
	THREE_D_ALTERNATE,
	/** This content is all the left frames of some 3D */
	THREE_D_LEFT,
	/** This content is all the right frames of some 3D */
	THREE_D_RIGHT
};


std::string video_frame_type_to_string(VideoFrameType);
VideoFrameType string_to_video_frame_type(std::string);


#endif

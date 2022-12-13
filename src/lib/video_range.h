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


#ifndef DCPOMATIC_VIDEO_RANGE_H
#define DCPOMATIC_VIDEO_RANGE_H


#include <string>


enum class VideoRange
{
	FULL, ///< full,  or "JPEG" (0-255 for 8-bit)
	VIDEO ///< video, or "MPEG" (16-235 for 8-bit)
};


extern std::string video_range_to_string(VideoRange r);
extern VideoRange string_to_video_range(std::string s);


#endif



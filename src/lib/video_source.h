/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/video_source.h
 *  @brief Parent class for classes which emit video data.
 */

#ifndef DCPOMATIC_VIDEO_SOURCE_H
#define DCPOMATIC_VIDEO_SOURCE_H

#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include "util.h"

class VideoSink;
class Subtitle;
class Image;

/** @class VideoSource
 *  @param A class that emits video data.
 */
class VideoSource
{
public:

	/** Emitted when a video frame is ready.
	 *  First parameter is the video image.
	 *  Second parameter is true if the image is the same as the last one that was emitted.
	 *  Third parameter is the time relative to the start of this source's content.
	 */
	boost::signals2::signal<void (boost::shared_ptr<const Image>, bool, Time)> Video;

	void connect_video (boost::shared_ptr<VideoSink>);
};

#endif

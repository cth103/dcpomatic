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

#ifndef DCPOMATIC_VIDEO_SINK_H
#define DCPOMATIC_VIDEO_SINK_H

#include <boost/shared_ptr.hpp>
#include "util.h"

class Subtitle;
class Image;

class VideoSink
{
public:
	/** Call with a frame of video.
	 *  @param i Video frame image.
	 *  @param same true if i is the same as last time we were called.
	 *  @param s A subtitle that should be on this frame, or 0.
	 */
	virtual void process_video (boost::shared_ptr<const Image> i, bool same, boost::shared_ptr<Subtitle> s) = 0;
};

class TimedVideoSink
{
public:
	/** Call with a frame of video.
	 *  @param i Video frame image.
	 *  @param same true if i is the same as last time we were called.
	 *  @param s A subtitle that should be on this frame, or 0.
	 *  @param t Source timestamp.
	 */
	virtual void process_video (boost::shared_ptr<const Image> i, bool same, boost::shared_ptr<Subtitle> s, double t) = 0;
};

#endif

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TYPES_H
#define DCPOMATIC_TYPES_H

#include <vector>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <libdcp/util.h>

class Content;
class AudioBuffers;

typedef int64_t Time;
#define TIME_MAX INT64_MAX
#define TIME_HZ	 ((Time) 96000)
typedef int64_t OutputAudioFrame;
typedef int	OutputVideoFrame;
typedef std::vector<boost::shared_ptr<Content> > ContentList;

template<class T>
struct TimedAudioBuffers
{
	TimedAudioBuffers ()
		: time (0)
	{}
	
	TimedAudioBuffers (boost::shared_ptr<AudioBuffers> a, T t)
		: audio (a)
		, time (t)
	{}
	
	boost::shared_ptr<AudioBuffers> audio;
	T time;
};

enum VideoFrameType
{
	VIDEO_FRAME_TYPE_2D,
	VIDEO_FRAME_TYPE_3D_LEFT_RIGHT
};

enum Eyes
{
	EYES_BOTH,
	EYES_LEFT,
	EYES_RIGHT,
	EYES_COUNT
};

/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
struct Crop
{
	Crop () : left (0), right (0), top (0), bottom (0) {}
	Crop (int l, int r, int t, int b) : left (l), right (r), top (t), bottom (b) {}

	/** Number of pixels to remove from the left-hand side */
	int left;
	/** Number of pixels to remove from the right-hand side */
	int right;
	/** Number of pixels to remove from the top */
	int top;
	/** Number of pixels to remove from the bottom */
	int bottom;
};

extern bool operator== (Crop const & a, Crop const & b);
extern bool operator!= (Crop const & a, Crop const & b);

enum Resolution {
	RESOLUTION_2K,
	RESOLUTION_4K
};

std::string resolution_to_string (Resolution);
Resolution string_to_resolution (std::string);

#endif

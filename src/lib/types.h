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

typedef int64_t ContentAudioFrame;
typedef int     ContentVideoFrame;
typedef int64_t Time;
#define TIME_MAX INT64_MAX
#define TIME_HZ  ((Time) 96000)
typedef int64_t OutputAudioFrame;
typedef int     OutputVideoFrame;

/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
struct Crop
{
	Crop () : left (0), right (0), top (0), bottom (0) {}

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

/** @struct Position
 *  @brief A position.
 */
struct Position
{
	Position ()
		: x (0)
		, y (0)
	{}

	Position (int x_, int y_)
		: x (x_)
		, y (y_)
	{}

	/** x coordinate */
	int x;
	/** y coordinate */
	int y;
};

/** @struct Rect
 *  @brief A rectangle.
 */
struct Rect
{
	Rect ()
		: x (0)
		, y (0)
		, width (0)
		, height (0)
	{}

	Rect (int x_, int y_, int w_, int h_)
		: x (x_)
		, y (y_)
		, width (w_)
		, height (h_)
	{}

	int x;
	int y;
	int width;
	int height;

	Position position () const {
		return Position (x, y);
	}

	libdcp::Size size () const {
		return libdcp::Size (width, height);
	}

	Rect intersection (Rect const & other) const;

	bool contains (Position) const;
};

#endif

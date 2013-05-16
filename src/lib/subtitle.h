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

/** @file  src/subtitle.h
 *  @brief Representations of subtitles.
 */

#include <list>
#include <boost/shared_ptr.hpp>
#include "types.h"

struct AVSubtitle;
class Image;

/** A subtitle, consisting of an image and a position */
class Subtitle
{
public:
	Subtitle (Position p, boost::shared_ptr<Image> i);

	void set_position (Position p) {
		_position = p;
	}

	Position position () const {
		return _position;
	}
	
	boost::shared_ptr<Image> image () const {
		return _image;
	}

	Rect area () const;
	
private:
	Position _position;
	boost::shared_ptr<Image> _image;
};

Rect
subtitle_transformed_area (
	float target_x_scale, float target_y_scale,
	Rect sub_area, int subtitle_offset, float subtitle_scale
	);

/** A Subtitle class with details of the time over which it should be shown */
class TimedSubtitle
{
public:
	TimedSubtitle (AVSubtitle const &);

	bool displayed_at (double t) const;
	
	boost::shared_ptr<Subtitle> subtitle () const {
		return _subtitle;
	}

private:
	/** the subtitle */
	boost::shared_ptr<Subtitle> _subtitle;
	/** display from time in seconds from the start of the film */
	double _from;
	/** display to time in seconds from the start of the film */
	double _to;
};

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

#include <list>
#include <boost/shared_ptr.hpp>
#include "util.h"

struct AVSubtitle;
class Image;

class Subtitle
{
public:
	Subtitle (AVSubtitle const &);
	Subtitle (Position p, boost::shared_ptr<Image> i);

	bool displayed_at (double t);

	void set_position (Position p) {
		_position = p;
	}

	Position position () const {
		return _position;
	}
	
	boost::shared_ptr<Image> image () const {
		return _image;
	}

	Rectangle area () const;
	
private:
	/** display from time in seconds from the start of the film */
	double _from;
	/** display to time in seconds from the start of the film */
	double _to;
	Position _position;
	boost::shared_ptr<Image> _image;
};

Rectangle
subtitle_transformed_area (
	float target_x_scale, float target_y_scale,
	Rectangle sub_area, int subtitle_offset, float subtitle_scale
	);
	

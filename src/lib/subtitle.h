/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <libdcp/util.h>
#include "rect.h"
#include "types.h"

class Film;
class Piece;
class Image;

class Subtitle
{
public:

	Subtitle (boost::shared_ptr<const Film>, libdcp::Size, boost::weak_ptr<Piece>, boost::shared_ptr<Image>, dcpomatic::Rect<double>, Time, Time);

	void update (boost::shared_ptr<const Film>, libdcp::Size);

	bool covers (Time t) const;

	boost::shared_ptr<Image> out_image () const {
		return _out_image;
	}

	Position<int> out_position () const {
		return _out_position;
	}
	
private:	
	boost::weak_ptr<Piece> _piece;
	boost::shared_ptr<Image> _in_image;
	dcpomatic::Rect<double> _in_rect;
	Time _in_from;
	Time _in_to;

	boost::shared_ptr<Image> _out_image;
	Position<int> _out_position;
	Time _out_from;
	Time _out_to;
};

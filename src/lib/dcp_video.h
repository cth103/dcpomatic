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

extern "C" {
#include <libavutil/avutil.h>
}
#include <boost/shared_ptr.hpp>
#include "types.h"
#include "colour_conversion.h"
#include "position.h"
#include "position_image.h"

class Image;
class Scaler;

/** @class DCPVideo
 *
 *  A ContentVideo image with:
 *     - content parameters (crop, scaling, colour conversion)
 *     - merged content (subtitles)
 *  and with its time converted from a ContentTime to a DCPTime.
 */
class DCPVideo
{
public:
	DCPVideo (boost::shared_ptr<const Image>, Eyes eyes, Crop, dcp::Size, dcp::Size, Scaler const *, ColourConversion conversion, DCPTime time);

	void set_subtitle (PositionImage);
	boost::shared_ptr<Image> image (AVPixelFormat, bool) const;

	Eyes eyes () const {
		return _eyes;
	}

	ColourConversion conversion () const {
		return _conversion;
	}

private:
	boost::shared_ptr<const Image> _in;
	Eyes _eyes;
	Crop _crop;
	dcp::Size _inter_size;
	dcp::Size _out_size;
	Scaler const * _scaler;
	ColourConversion _conversion;
	DCPTime _time;
	PositionImage _subtitle;
};

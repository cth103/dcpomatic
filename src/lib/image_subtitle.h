/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_IMAGE_SUBTITLE_H
#define DCPOMATIC_IMAGE_SUBTITLE_H

#include "rect.h"
#include <boost/shared_ptr.hpp>

class Image;

class BitmapText
{
public:
	BitmapText (boost::shared_ptr<Image> i, dcpomatic::Rect<double> r)
		: image (i)
		, rectangle (r)
	{}

	boost::shared_ptr<Image> image;
	/** Area that the subtitle covers on its corresponding video, expressed in
	 *  proportions of the image size; e.g. rectangle.x = 0.5 would mean that
	 *  the rectangle starts half-way across the video.
	 *
	 *  This rectangle may or may not have had a TextContent's offsets and
	 *  scale applied to it, depending on context.
	 */
	dcpomatic::Rect<double> rectangle;
};

#endif

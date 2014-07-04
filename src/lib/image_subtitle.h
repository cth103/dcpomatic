/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_IMAGE_SUBTITLE_H
#define DCPOMATIC_IMAGE_SUBTITLE_H

#include "rect.h"

class Image;

class ImageSubtitle
{
public:
	ImageSubtitle (boost::shared_ptr<Image> i, dcpomatic::Rect<double> r)
		: image (i)
		, rectangle (r)
	{}
	
	boost::shared_ptr<Image> image;
	/** Area that the subtitle covers on its corresponding video, expressed in
	 *  proportions of the image size; e.g. rectangle.x = 0.5 would mean that
	 *  the rectangle starts half-way across the video.
	 *
	 *  This rectangle may or may not have had a SubtitleContent's offsets and
	 *  scale applied to it, depending on context.
	 */
	dcpomatic::Rect<double> rectangle;
};

#endif

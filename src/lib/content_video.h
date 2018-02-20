/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_CONTENT_VIDEO_H
#define DCPOMATIC_CONTENT_VIDEO_H

#include "types.h"

class ImageProxy;

/** @class ContentVideo
 *  @brief A frame of video straight out of some content.
 */
class ContentVideo
{
public:
	ContentVideo ()
		: frame (0)
		, eyes (EYES_LEFT)
		, part (PART_WHOLE)
	{}

	ContentVideo (boost::shared_ptr<const ImageProxy> i, Frame f, Eyes e, Part p)
		: image (i)
		, frame (f)
		, eyes (e)
		, part (p)
	{}

	boost::shared_ptr<const ImageProxy> image;
	Frame frame;
	Eyes eyes;
	Part part;
};

#endif

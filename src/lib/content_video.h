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

#ifndef DCPOMATIC_CONTENT_VIDEO_H
#define DCPOMATIC_CONTENT_VIDEO_H

class ImageProxy;

/** @class ContentVideo
 *  @brief A frame of video straight out of some content.
 */
class ContentVideo
{
public:
	ContentVideo ()
		: eyes (EYES_BOTH)
		, part (PART_WHOLE)
	{}

	ContentVideo (boost::shared_ptr<const ImageProxy> i, Eyes e, Part p, VideoFrame f)
		: image (i)
		, eyes (e)
		, part (p)
		, frame (f)
	{}
	
	boost::shared_ptr<const ImageProxy> image;
	Eyes eyes;
	Part part;
	VideoFrame frame;
};

#endif

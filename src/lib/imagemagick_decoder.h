/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include "video_decoder.h"
#include "imagemagick.h"

namespace Magick {
	class Image;
}

class ImageMagickContent;

class ImageMagickDecoder : public VideoDecoder, public ImageMagick
{
public:
	ImageMagickDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const ImageMagickContent>);

	/* Decoder */

	void pass ();
	void seek (VideoContent::Frame);
	void seek_back ();
	bool done () const;

private:
	boost::shared_ptr<Image> _image;
	mutable boost::optional<libdcp::Size> _video_size;
};

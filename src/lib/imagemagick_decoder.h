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

#include "video_decoder.h"

namespace Magick {
	class Image;
}

class ImageMagickContent;

class ImageMagickDecoder : public VideoDecoder
{
public:
	ImageMagickDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const ImageMagickContent>);

	float video_frame_rate () const {
		return 24;
	}

	libdcp::Size native_size () const;
	ContentVideoFrame video_length () const;

	bool seek (double);
	bool pass ();

protected:
	PixelFormat pixel_format () const;

	int time_base_numerator () const {
		return 0;
	}

	int time_base_denominator () const {
		return 0;
	}

	int sample_aspect_ratio_numerator () const {
		/* XXX */
		return 1;
	}

	int sample_aspect_ratio_denominator () const {
		/* XXX */
		return 1;
	}

private:
	boost::shared_ptr<const ImageMagickContent> _imagemagick_content;
	ContentVideoFrame _position;
};

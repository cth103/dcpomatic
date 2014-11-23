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

#include <iostream>
#include <boost/filesystem.hpp>
#include <Magick++.h>
#include "image_content.h"
#include "image_decoder.h"
#include "image.h"
#include "magick_image_proxy.h"
#include "j2k_image_proxy.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using dcp::Size;

ImageDecoder::ImageDecoder (shared_ptr<const ImageContent> c)
	: VideoDecoder (c)
	, _image_content (c)
{

}

bool
ImageDecoder::pass ()
{
	if (_video_position >= _image_content->video_length().frames (_image_content->video_frame_rate ())) {
		return true;
	}

	if (!_image_content->still() || !_image) {
		/* Either we need an image or we are using moving images, so load one */
		boost::filesystem::path path = _image_content->path (_image_content->still() ? 0 : _video_position);
		if (valid_j2k_file (path)) {
			/* We can't extract image size from a JPEG2000 codestream without decoding it,
			   so pass in the image content's size here.
			*/
			_image.reset (new J2KImageProxy (path, _image_content->video_size ()));
		} else {
			_image.reset (new MagickImageProxy (path));
		}
	}
		
	video (_image, _video_position);
	++_video_position;
	return false;
}

void
ImageDecoder::seek (ContentTime time, bool accurate)
{
	VideoDecoder::seek (time, accurate);
	_video_position = time.frames (_image_content->video_frame_rate ());
}

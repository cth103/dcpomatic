/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "image_content.h"
#include "image_decoder.h"
#include "video_decoder.h"
#include "image.h"
#include "magick_image_proxy.h"
#include "j2k_image_proxy.h"
#include "film.h"
#include "exceptions.h"
#include "video_content.h"
#include <Magick++.h>
#include <boost/filesystem.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using dcp::Size;

ImageDecoder::ImageDecoder (shared_ptr<const ImageContent> c, shared_ptr<Log> log)
	: _image_content (c)
	, _frame_video_position (0)
{
	video.reset (new VideoDecoder (this, c, log));
}

void
ImageDecoder::pass ()
{
	if (_frame_video_position >= _image_content->video->length()) {
		return;
	}

	if (!_image_content->still() || !_image) {
		/* Either we need an image or we are using moving images, so load one */
		boost::filesystem::path path = _image_content->path (_image_content->still() ? 0 : _frame_video_position);
		if (valid_j2k_file (path)) {
			AVPixelFormat pf;
			if (_image_content->video->colour_conversion()) {
				/* We have a specified colour conversion: assume the image is RGB */
				pf = AV_PIX_FMT_RGB48LE;
			} else {
				/* No specified colour conversion: assume the image is XYZ */
				pf = AV_PIX_FMT_XYZ12LE;
			}
			/* We can't extract image size from a JPEG2000 codestream without decoding it,
			   so pass in the image content's size here.
			*/
			_image.reset (new J2KImageProxy (path, _image_content->video->size(), pf));
		} else {
			_image.reset (new MagickImageProxy (path));
		}
	}

	video->set_position (ContentTime::from_frames (_frame_video_position, _image_content->active_video_frame_rate ()));
	video->emit (_image, _frame_video_position);
	++_frame_video_position;
	return;
}

void
ImageDecoder::seek (ContentTime time, bool)
{
	_frame_video_position = time.frames_round (_image_content->active_video_frame_rate ());
}

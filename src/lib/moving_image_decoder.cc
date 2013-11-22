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
#include "moving_image_content.h"
#include "moving_image_decoder.h"
#include "image.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using libdcp::Size;

MovingImageDecoder::MovingImageDecoder (shared_ptr<const Film> f, shared_ptr<const MovingImageContent> c)
	: Decoder (f)
	, VideoDecoder (f, c)
	, MovingImage (c)
{

}

void
MovingImageDecoder::pass ()
{
	if (_video_position >= _moving_image_content->video_length ()) {
		return;
	}

	Magick::Image* magick_image = new Magick::Image (_moving_image_content->path(_video_position).string ());
	libdcp::Size size (magick_image->columns(), magick_image->rows());

	shared_ptr<Image> image (new Image (PIX_FMT_RGB24, size, true));

	using namespace MagickCore;
	
	uint8_t* p = image->data()[0];
	for (int y = 0; y < size.height; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = magick_image->pixelColor (x, y);
			*q++ = c.redQuantum() * 255 / QuantumRange;
			*q++ = c.greenQuantum() * 255 / QuantumRange;
			*q++ = c.blueQuantum() * 255 / QuantumRange;
		}
		p += image->stride()[0];
	}

	delete magick_image;

	video (image, false, _video_position);
}

void
MovingImageDecoder::seek (VideoContent::Frame frame, bool)
{
	_video_position = frame;
}

bool
MovingImageDecoder::done () const
{
	return _video_position >= _moving_image_content->video_length ();
}

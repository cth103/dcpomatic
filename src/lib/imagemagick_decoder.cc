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

#include <iostream>
#include <Magick++.h>
#include "imagemagick_decoder.h"
#include "image.h"
#include "film.h"

using namespace std;
using namespace boost;

ImageMagickDecoder::ImageMagickDecoder (
	boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j, bool minimal)
	: Decoder (f, o, j, minimal)
	, _done (false)
{
	_magick_image = new Magick::Image (_film->content_path ());
}

Size
ImageMagickDecoder::native_size () const
{
	return Size (_magick_image->columns(), _magick_image->rows());
}

bool
ImageMagickDecoder::pass ()
{
	using namespace MagickCore;
	
	if (_done) {
		return true;
	}

	Size size = native_size ();
	RGBFrameImage image (size);

	uint8_t* p = image.data()[0];
	for (int y = 0; y < size.height; ++y) {
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = _magick_image->pixelColor (x, y);
			*p++ = c.redQuantum() * 255 / QuantumRange;
			*p++ = c.greenQuantum() * 255 / QuantumRange;
			*p++ = c.blueQuantum() * 255 / QuantumRange;
		}

	}
	
	process_video (image.frame ());

	_done = true;
	return false;
}

PixelFormat
ImageMagickDecoder::pixel_format () const
{
	return PIX_FMT_RGB24;
}


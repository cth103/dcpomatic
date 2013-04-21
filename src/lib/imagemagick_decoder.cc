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
#include <boost/filesystem.hpp>
#include <Magick++.h>
#include "imagemagick_content.h"
#include "imagemagick_decoder.h"
#include "image.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using libdcp::Size;

ImageMagickDecoder::ImageMagickDecoder (shared_ptr<const Film> f, shared_ptr<const ImageMagickContent> c)
	: Decoder (f)
	, VideoDecoder (f)
	, _imagemagick_content (c)
	, _position (0)
{
	
}

libdcp::Size
ImageMagickDecoder::native_size () const
{
	using namespace MagickCore;
	Magick::Image* image = new Magick::Image (_imagemagick_content->file().string());
	libdcp::Size const s = libdcp::Size (image->columns(), image->rows());
	delete image;

	return s;
}

int
ImageMagickDecoder::video_length () const
{
	return _imagemagick_content->video_length ();
}

bool
ImageMagickDecoder::pass ()
{
	if (_position < 0 || _position >= _imagemagick_content->video_length ()) {
		return true;
	}

	if (_image) {
		emit_video (_image, true, double (_position) / 24);
		_position++;
		return false;
	}
	
	Magick::Image* magick_image = new Magick::Image (_imagemagick_content->file().string ());
	
	libdcp::Size size = native_size ();
	_image.reset (new SimpleImage (PIX_FMT_RGB24, size, false));

	using namespace MagickCore;
	
	uint8_t* p = _image->data()[0];
	for (int y = 0; y < size.height; ++y) {
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = magick_image->pixelColor (x, y);
			*p++ = c.redQuantum() * 255 / QuantumRange;
			*p++ = c.greenQuantum() * 255 / QuantumRange;
			*p++ = c.blueQuantum() * 255 / QuantumRange;
		}
	}

	delete magick_image;

	_image = _image->crop (_film->crop(), true);
	emit_video (_image, false, double (_position) / 24);

	++_position;
	return false;
}

PixelFormat
ImageMagickDecoder::pixel_format () const
{
	/* XXX: always true? */
	return PIX_FMT_RGB24;
}

bool
ImageMagickDecoder::seek (double t)
{
	int const f = t * _imagemagick_content->video_frame_rate ();

	if (f >= _imagemagick_content->video_length()) {
		_position = 0;
		return true;
	}

	_position = f;
	return false;
}

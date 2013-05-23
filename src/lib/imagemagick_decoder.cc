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
	, VideoDecoder (f, c)
	, _imagemagick_content (c)
{

}

libdcp::Size
ImageMagickDecoder::video_size () const
{
	if (!_video_size) {
		using namespace MagickCore;
		Magick::Image* image = new Magick::Image (_imagemagick_content->file().string());
		_video_size = libdcp::Size (image->columns(), image->rows());
		delete image;
	}

	return _video_size.get ();
}

int
ImageMagickDecoder::video_length () const
{
	return _imagemagick_content->video_length ();
}

float
ImageMagickDecoder::video_frame_rate () const
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return 24;
	}

	return f->dcp_video_frame_rate ();
}

void
ImageMagickDecoder::pass ()
{
	if (_next_video >= _imagemagick_content->video_length ()) {
		return;
	}

	if (_image) {
		video (_image, true, _next_video);
		return;
	}

	Magick::Image* magick_image = new Magick::Image (_imagemagick_content->file().string ());
	_video_size = libdcp::Size (magick_image->columns(), magick_image->rows());
	
	_image.reset (new SimpleImage (PIX_FMT_RGB24, _video_size.get(), false));

	using namespace MagickCore;
	
	uint8_t* p = _image->data()[0];
	for (int y = 0; y < _video_size->height; ++y) {
		for (int x = 0; x < _video_size->width; ++x) {
			Magick::Color c = magick_image->pixelColor (x, y);
			*p++ = c.redQuantum() * 255 / QuantumRange;
			*p++ = c.greenQuantum() * 255 / QuantumRange;
			*p++ = c.blueQuantum() * 255 / QuantumRange;
		}
	}

	delete magick_image;

	_image = _image->crop (_imagemagick_content->crop(), true);
	video (_image, false, _next_video);
}

void
ImageMagickDecoder::seek (Time t)
{
	_next_video = t;
}

Time
ImageMagickDecoder::next () const
{
	return _next_video;
}

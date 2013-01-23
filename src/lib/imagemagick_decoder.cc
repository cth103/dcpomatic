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
#include "imagemagick_decoder.h"
#include "image.h"
#include "film.h"
#include "exceptions.h"

using std::cout;
using boost::shared_ptr;
using libdcp::Size;

ImageMagickDecoder::ImageMagickDecoder (
	boost::shared_ptr<Film> f, DecodeOptions o, Job* j)
	: Decoder (f, o, j)
	, VideoDecoder (f, o, j)
{
	if (boost::filesystem::is_directory (_film->content_path())) {
		for (
			boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (_film->content_path());
			i != boost::filesystem::directory_iterator();
			++i) {

			if (still_image_file (i->path().string())) {
				_files.push_back (i->path().string());
			}
		}
	} else {
		_files.push_back (_film->content_path ());
	}

	_iter = _files.begin ();
}

Size
ImageMagickDecoder::native_size () const
{
	if (_files.empty ()) {
		throw DecodeError ("no still image files found");
	}

	/* Look at the first file and assume its size holds for all */
	using namespace MagickCore;
	Magick::Image* image = new Magick::Image (_film->content_path ());
	Size const s = Size (image->columns(), image->rows());
	delete image;

	return s;
}

bool
ImageMagickDecoder::pass ()
{
	if (_iter == _files.end()) {
		if (video_frame() >= _film->still_duration_in_frames()) {
			return true;
		}

		repeat_last_video ();
		return false;
	}
	
	Magick::Image* magick_image = new Magick::Image (_film->content_path ());
	
	Size size = native_size ();
	shared_ptr<Image> image (new SimpleImage (PIX_FMT_RGB24, size, false));

	using namespace MagickCore;
	
	uint8_t* p = image->data()[0];
	for (int y = 0; y < size.height; ++y) {
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = magick_image->pixelColor (x, y);
			*p++ = c.redQuantum() * 255 / QuantumRange;
			*p++ = c.greenQuantum() * 255 / QuantumRange;
			*p++ = c.blueQuantum() * 255 / QuantumRange;
		}
	}

	delete magick_image;

	image = image->crop (_film->crop(), true);
	
	emit_video (image, 0);

	++_iter;
	return false;
}

PixelFormat
ImageMagickDecoder::pixel_format () const
{
	/* XXX: always true? */
	return PIX_FMT_RGB24;
}

bool
ImageMagickDecoder::seek_to_last ()
{
	if (_iter == _files.end()) {
		_iter = _files.begin();
	} else {
		--_iter;
	}

	return false;
}

bool
ImageMagickDecoder::seek (double t)
{
	int const f = t * frames_per_second();
	
	_iter = _files.begin ();
	for (int i = 0; i < f; ++i) {
		if (_iter == _files.end()) {
			return true;
		}
		++_iter;
	}
	
	return false;
}

void
ImageMagickDecoder::film_changed (Film::Property p)
{
	if (p == Film::CROP) {
		OutputChanged ();
	}
}

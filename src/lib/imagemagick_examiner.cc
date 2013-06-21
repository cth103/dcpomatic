/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "imagemagick_content.h"
#include "imagemagick_examiner.h"
#include "film.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;

ImageMagickExaminer::ImageMagickExaminer (shared_ptr<const Film> f, shared_ptr<const ImageMagickContent> c)
	: ImageMagick (c)
	, _film (f)
{
	using namespace MagickCore;
	Magick::Image* image = new Magick::Image (_imagemagick_content->file().string());
	_video_size = libdcp::Size (image->columns(), image->rows());
	delete image;
}

libdcp::Size
ImageMagickExaminer::video_size () const
{
	return _video_size;
}

int
ImageMagickExaminer::video_length () const
{
	return _imagemagick_content->video_length ();
}

float
ImageMagickExaminer::video_frame_rate () const
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return 24;
	}

	return f->dcp_video_frame_rate ();
}


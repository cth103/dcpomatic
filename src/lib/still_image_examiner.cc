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
#include "still_image_content.h"
#include "still_image_examiner.h"
#include "film.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;

StillImageExaminer::StillImageExaminer (shared_ptr<const Film> f, shared_ptr<const StillImageContent> c)
	: StillImage (c)
	, _film (f)
{
	using namespace MagickCore;
	Magick::Image* image = new Magick::Image (_still_image_content->path().string());
	_video_size = libdcp::Size (image->columns(), image->rows());
	delete image;
}

libdcp::Size
StillImageExaminer::video_size () const
{
	return _video_size;
}

int
StillImageExaminer::video_length () const
{
	return _still_image_content->video_length ();
}

float
StillImageExaminer::video_frame_rate () const
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return 24;
	}

	return f->video_frame_rate ();
}


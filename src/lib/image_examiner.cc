/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "image_content.h"
#include "image_examiner.h"
#include "film.h"
#include "job.h"
#include "exceptions.h"
#include "config.h"
#include "cross.h"
#include "compose.hpp"
#include <dcp/openjpeg_image.h>
#include <dcp/exceptions.h>
#include <dcp/j2k.h>
#include <Magick++.h>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::list;
using std::sort;
using boost::shared_ptr;
using boost::optional;

ImageExaminer::ImageExaminer (shared_ptr<const Film> film, shared_ptr<const ImageContent> content, shared_ptr<Job>)
	: _film (film)
	, _image_content (content)
{
#ifdef DCPOMATIC_HAVE_MAGICKCORE_NAMESPACE
	using namespace MagickCore;
#endif
	boost::filesystem::path path = content->path(0).string ();
	if (valid_j2k_file (path)) {
		boost::uintmax_t size = boost::filesystem::file_size (path);
		FILE* f = fopen_boost (path, "rb");
		if (!f) {
			throw FileError ("Could not open file for reading", path);
		}
		uint8_t* buffer = new uint8_t[size];
		fread (buffer, 1, size, f);
		fclose (f);
		try {
			_video_size = dcp::decompress_j2k (buffer, size, 0)->size ();
		} catch (dcp::DCPReadError& e) {
			delete[] buffer;
			throw DecodeError (String::compose (_("Could not decode JPEG2000 file %1 (%2)"), path, e.what ()));
		}
		delete[] buffer;
	} else {
		Magick::Image* image = new Magick::Image (content->path(0).string());
		_video_size = dcp::Size (image->columns(), image->rows());
		delete image;
	}

	if (content->still ()) {
		_video_length = Config::instance()->default_still_length() * video_frame_rate().get_value_or (film->video_frame_rate ());
	} else {
		_video_length = _image_content->number_of_paths ();
	}
}

dcp::Size
ImageExaminer::video_size () const
{
	return _video_size.get ();
}

optional<double>
ImageExaminer::video_frame_rate () const
{
	if (_image_content->video_frame_rate()) {
		/* The content already knows what frame rate it should be */
		return _image_content->video_frame_rate().get();
	}

	/* Don't know */
	return optional<double> ();
}

bool
ImageExaminer::yuv () const
{
	/* We never convert ImageSource from YUV to RGB (though maybe sometimes we should)
	   so it makes sense to just say they are never YUV so the option of a conversion
	   to RGB is not offered.
	*/
	return false;
}

/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
#include <dcp/xyz_frame.h>
#include <dcp/exceptions.h>
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
#ifdef DCPOMATIC_IMAGE_MAGICK	
	using namespace MagickCore;
#endif
	boost::filesystem::path path = content->path(0).string ();
	if (valid_j2k_file (path)) {
		boost::uintmax_t size = boost::filesystem::file_size (path);
		FILE* f = fopen_boost (path, "r");
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
		_video_length = ContentTime::from_seconds (Config::instance()->default_still_length());
	} else {
		_video_length = ContentTime::from_frames (
			_image_content->number_of_paths (), video_frame_rate().get_value_or (24)
			);
	}
}

dcp::Size
ImageExaminer::video_size () const
{
	return _video_size.get ();
}

optional<float>
ImageExaminer::video_frame_rate () const
{
	/* Don't know */
	return optional<float> ();
}

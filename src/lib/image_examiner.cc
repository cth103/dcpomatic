/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "config.h"
#include "cross.h"
#include "exceptions.h"
#include "ffmpeg_image_proxy.h"
#include "film.h"
#include "image.h"
#include "image_content.h"
#include "image_examiner.h"
#include "job.h"
#include <dcp/openjpeg_image.h>
#include <dcp/exceptions.h>
#include <dcp/j2k_transcode.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::list;
using std::shared_ptr;
using std::sort;
using boost::optional;


ImageExaminer::ImageExaminer (shared_ptr<const Film> film, shared_ptr<const ImageContent> content, shared_ptr<Job>)
	: _film (film)
	, _image_content (content)
{
	auto path = content->path(0);
	if (valid_j2k_file (path)) {
		auto size = boost::filesystem::file_size (path);
		auto f = fopen_boost (path, "rb");
		if (!f) {
			throw FileError ("Could not open file for reading", path);
		}
		std::vector<uint8_t> buffer(size);
		checked_fread (buffer.data(), size, f, path);
		fclose (f);
		try {
			_video_size = dcp::decompress_j2k(buffer.data(), size, 0)->size();
		} catch (dcp::ReadError& e) {
			throw DecodeError (String::compose (_("Could not decode JPEG2000 file %1 (%2)"), path, e.what ()));
		}
	} else {
		FFmpegImageProxy proxy(content->path(0));
		_video_size = proxy.image(Image::Alignment::COMPACT).image->size();
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
	return {};
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

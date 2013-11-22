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
#include <boost/lexical_cast.hpp>
#include <Magick++.h>
#include "image_content.h"
#include "image_examiner.h"
#include "film.h"
#include "job.h"
#include "exceptions.h"
#include "config.h"

#include "i18n.h"

using std::cout;
using std::list;
using std::sort;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::bad_lexical_cast;

ImageExaminer::ImageExaminer (shared_ptr<const Film> film, shared_ptr<const ImageContent> content, shared_ptr<Job> job)
	: _film (film)
	, _image_content (content)
	, _video_length (0)
{
	list<unsigned int> frames;
	size_t const N = content->number_of_paths ();

	for (size_t i = 0; i < N; ++i) {
		boost::filesystem::path const p = content->path (i);
		try {
			frames.push_back (lexical_cast<int> (p.stem().string()));
		} catch (bad_lexical_cast &) {
			/* We couldn't turn that filename into a number; never mind */
		}
		
		if (!_video_size) {
			using namespace MagickCore;
			Magick::Image* image = new Magick::Image (p.string());
			_video_size = libdcp::Size (image->columns(), image->rows());
			delete image;
		}
	
		job->set_progress (float (i) / N);
	}

	frames.sort ();
	
	if (N > 1 && frames.front() != 0 && frames.front() != 1) {
		throw StringError (String::compose (_("first frame in moving image directory is number %1"), frames.front ()));
	}

	if (N > 1 && frames.back() != frames.size() && frames.back() != (frames.size() - 1)) {
		throw StringError (String::compose (_("there are %1 images in the directory but the last one is number %2"), frames.size(), frames.back ()));
	}

	if (content->still ()) {
		_video_length = Config::instance()->default_still_length() * video_frame_rate();
	} else {
		_video_length = _image_content->number_of_paths ();
	}
}

libdcp::Size
ImageExaminer::video_size () const
{
	return _video_size.get ();
}

float
ImageExaminer::video_frame_rate () const
{
	boost::shared_ptr<const Film> f = _film.lock ();
	if (!f) {
		return 24;
	}

	return f->video_frame_rate ();
}

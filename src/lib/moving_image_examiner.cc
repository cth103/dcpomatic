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
#include "moving_image_content.h"
#include "moving_image_examiner.h"
#include "film.h"
#include "job.h"
#include "exceptions.h"

#include "i18n.h"

using std::cout;
using std::list;
using std::sort;
using boost::shared_ptr;
using boost::lexical_cast;

MovingImageExaminer::MovingImageExaminer (shared_ptr<const Film> film, shared_ptr<const MovingImageContent> content, shared_ptr<Job> job)
	: MovingImage (content)
	, _film (film)
	, _video_length (0)
{
	list<unsigned int> frames;
	size_t const N = content->number_of_paths ();

	int j = 0;
	for (size_t i = 0; i < N; ++i) {
		boost::filesystem::path const p = content->path (i);
		frames.push_back (lexical_cast<int> (p.stem().string()));
		
		if (!_video_size) {
			using namespace MagickCore;
			Magick::Image* image = new Magick::Image (p.string());
			_video_size = libdcp::Size (image->columns(), image->rows());
			delete image;
		}
	
		job->set_progress (float (i) / N);
	}

	frames.sort ();
	
	if (frames.size() < 2) {
		throw StringError (String::compose (_("only %1 file(s) found in moving image directory"), frames.size ()));
	}

	if (frames.front() != 0 && frames.front() != 1) {
		throw StringError (String::compose (_("first frame in moving image directory is number %1"), frames.front ()));
	}

	if (frames.back() != frames.size() && frames.back() != (frames.size() - 1)) {
		throw StringError (String::compose (_("there are %1 images in the directory but the last one is number %2"), frames.size(), frames.back ()));
	}

	_video_length = frames.size ();
}

libdcp::Size
MovingImageExaminer::video_size () const
{
	return _video_size.get ();
}

int
MovingImageExaminer::video_length () const
{
	return _video_length;
}

float
MovingImageExaminer::video_frame_rate () const
{
	return 24;
}


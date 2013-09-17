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

#include "moving_image.h"
#include "video_examiner.h"

namespace Magick {
	class Image;
}

class MovingImageContent;

class MovingImageExaminer : public MovingImage, public VideoExaminer
{
public:
	MovingImageExaminer (boost::shared_ptr<const Film>, boost::shared_ptr<const MovingImageContent>, boost::shared_ptr<Job>);

	float video_frame_rate () const;
	libdcp::Size video_size () const;
	VideoContent::Frame video_length () const;

	std::vector<boost::filesystem::path> const & files () const {
		return _files;
	}

private:
	boost::weak_ptr<const Film> _film;
	boost::optional<libdcp::Size> _video_size;
	VideoContent::Frame _video_length;
	std::vector<boost::filesystem::path> _files;
};

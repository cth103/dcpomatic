/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "video_examiner.h"

class ImageContent;

class ImageExaminer : public VideoExaminer
{
public:
	ImageExaminer (std::shared_ptr<const Film>, std::shared_ptr<const ImageContent>, std::shared_ptr<Job>);

	bool has_video () const {
		return true;
	}
	boost::optional<double> video_frame_rate () const;
	dcp::Size video_size () const;
	Frame video_length () const {
		return _video_length;
	}
	bool yuv () const;
	VideoRange range () const {
		return VideoRange::FULL;
	}

private:
	std::weak_ptr<const Film> _film;
	std::shared_ptr<const ImageContent> _image_content;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length;
};

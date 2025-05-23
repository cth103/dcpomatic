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

	bool has_video () const override {
		return true;
	}
	boost::optional<double> video_frame_rate () const override;
	boost::optional<dcp::Size> video_size() const override;
	Frame video_length () const override {
		return _video_length;
	}
	bool yuv () const override;
	VideoRange range () const override {
		return VideoRange::FULL;
	}
	PixelQuanta pixel_quanta () const override {
		/* See ::yuv - we're assuming the image is not YUV and so not subsampled */
		return {};
	}
	bool has_alpha() const override {
		return _has_alpha;
	}

private:
	std::weak_ptr<const Film> _film;
	std::shared_ptr<const ImageContent> _image_content;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length;
	bool _has_alpha = false;
};

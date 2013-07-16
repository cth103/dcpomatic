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

#ifndef DCPOMATIC_IMAGEMAGICK_CONTENT_H
#define DCPOMATIC_IMAGEMAGICK_CONTENT_H

#include <boost/enable_shared_from_this.hpp>
#include "video_content.h"

namespace cxml {
	class Node;
}

class ImageMagickContent : public VideoContent
{
public:
	ImageMagickContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	ImageMagickContent (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);

	boost::shared_ptr<ImageMagickContent> shared_from_this () {
		return boost::dynamic_pointer_cast<ImageMagickContent> (Content::shared_from_this ());
	};

	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	void as_xml (xmlpp::Node *) const;
	Time length () const;

	std::string identifier () const;
	
	void set_video_length (VideoContent::Frame);

	static bool valid_file (boost::filesystem::path);
};

#endif

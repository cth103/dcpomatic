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

#ifndef DCPOMATIC_IMAGE_CONTENT_H
#define DCPOMATIC_IMAGE_CONTENT_H

#include <boost/enable_shared_from_this.hpp>
#include "video_content.h"

namespace cxml {
	class Node;
}

class ImageContent : public VideoContent
{
public:
	ImageContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	ImageContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<ImageContent> shared_from_this () {
		return boost::dynamic_pointer_cast<ImageContent> (Content::shared_from_this ());
	};

	void examine (boost::shared_ptr<Job>, bool calculate_digest);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *) const;
	DCPTime full_length () const;

	std::string identifier () const;
	
	void set_video_length (ContentTime);
	bool still () const;
	void set_video_frame_rate (float);
};

#endif

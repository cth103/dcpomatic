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

#include <boost/enable_shared_from_this.hpp>
#include "video_content.h"

namespace cxml {
	class Node;
}

class ImageMagickContent : public VideoContent, public boost::enable_shared_from_this<ImageMagickContent>
{
public:
	ImageMagickContent (boost::filesystem::path);
	ImageMagickContent (boost::shared_ptr<const cxml::Node>);

	void examine (boost::shared_ptr<Film>, boost::shared_ptr<Job>, bool);
	std::string summary () const;
	void as_xml (xmlpp::Node *) const;
	boost::shared_ptr<Content> clone () const;

	static bool valid_file (boost::filesystem::path);
};

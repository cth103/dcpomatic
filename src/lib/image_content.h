/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_IMAGE_CONTENT_H
#define DCPOMATIC_IMAGE_CONTENT_H

#include "content.h"

class ImageContent : public Content
{
public:
	ImageContent (boost::filesystem::path);
	ImageContent (cxml::ConstNodePtr, int);

	std::shared_ptr<ImageContent> shared_from_this () {
		return std::dynamic_pointer_cast<ImageContent> (Content::shared_from_this ());
	};

	std::shared_ptr<const ImageContent> shared_from_this () const {
		return std::dynamic_pointer_cast<const ImageContent> (Content::shared_from_this ());
	};

	void examine (std::shared_ptr<const Film> film, std::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	dcpomatic::DCPTime full_length (std::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime approximate_length () const;

	std::string identifier () const;

	void set_default_colour_conversion ();

	bool still () const;

private:
	void add_properties (std::shared_ptr<const Film> film, std::list<UserProperty>& p) const;

	boost::optional<boost::filesystem::path> _path_to_scan;
};

#endif

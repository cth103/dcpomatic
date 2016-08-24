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
	ImageContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	ImageContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<ImageContent> shared_from_this () {
		return boost::dynamic_pointer_cast<ImageContent> (Content::shared_from_this ());
	};

	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	DCPTime full_length () const;

	std::string identifier () const;

	void set_default_colour_conversion ();

	bool still () const;

private:
	void add_properties (std::list<UserProperty>& p) const;
};

#endif

/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "content.h"

class AtmosMXFContent : public Content
{
public:
	AtmosMXFContent (boost::shared_ptr<const Film> film, boost::filesystem::path path);
	AtmosMXFContent (boost::shared_ptr<const Film> film, cxml::ConstNodePtr node, int version);

	boost::shared_ptr<AtmosMXFContent> shared_from_this () {
		return boost::dynamic_pointer_cast<AtmosMXFContent> (Content::shared_from_this ());
	}

	void examine (boost::shared_ptr<Job> job);
	std::string summary () const;
	void as_xml (xmlpp::Node* node) const;
	DCPTime full_length () const;

	static bool valid_mxf (boost::filesystem::path path);

private:
	Frame _length;
};

/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_content.h"

class SubRipContent : public SubtitleContent
{
public:
	SubRipContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	SubRipContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<SubRipContent> shared_from_this () {
		return boost::dynamic_pointer_cast<SubRipContent> (Content::shared_from_this ());
	}
	
	/* Content */
	void examine (boost::shared_ptr<Job>, bool calculate_digest);
	std::string summary () const;
	std::string technical_summary () const;
	std::string information () const;
	void as_xml (xmlpp::Node *) const;
	DCPTime full_length () const;

	/* SubtitleContent */
	bool has_subtitles () const {
		return true;
	}

private:
	DCPTime _length;
};

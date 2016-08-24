/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "content.h"

class Job;

/** @class TextSubtitleContent
 *  @brief SubRip or SSA subtitles.
 */
class TextSubtitleContent : public Content
{
public:
	TextSubtitleContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	TextSubtitleContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<TextSubtitleContent> shared_from_this () {
		return boost::dynamic_pointer_cast<TextSubtitleContent> (Content::shared_from_this ());
	}

	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	DCPTime full_length () const;

private:
	ContentTime _length;
};

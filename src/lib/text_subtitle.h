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

#ifndef DCPOMATIC_TEXT_SUBTITLE_H
#define DCPOMATIC_TEXT_SUBTITLE_H

#include "dcpomatic_time.h"
#include <sub/subtitle.h>
#include <boost/shared_ptr.hpp>
#include <vector>

class TextSubtitleContent;
class text_subtitle_time_test;
class text_subtitle_coordinate_test;
class text_subtitle_content_test;
class text_subtitle_parse_test;

class TextSubtitle
{
public:
	explicit TextSubtitle (boost::shared_ptr<const TextSubtitleContent>);

	boost::optional<ContentTime> first () const;
	ContentTime length () const;

protected:
	std::vector<sub::Subtitle> _subtitles;
};

#endif

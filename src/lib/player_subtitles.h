/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PLAYER_SUBTITLES_H
#define DCPOMATIC_PLAYER_SUBTITLES_H

#include "image_subtitle.h"
#include "dcpomatic_time.h"
#include "subtitle_string.h"

class Font;

/** A set of subtitles which span the same time period */
class PlayerSubtitles
{
public:
	void add_fonts (std::list<boost::shared_ptr<Font> > fonts_);
	std::list<boost::shared_ptr<Font> > fonts;

	DCPTimePeriod period;

	/** ImageSubtitles, with their rectangles transformed as specified by their content */
	std::list<ImageSubtitle> image;
	std::list<SubtitleString> text;
};

#endif

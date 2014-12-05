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

#ifndef DCPOMATIC_PLAYER_SUBTITLES_H
#define DCPOMATIC_PLAYER_SUBTITLES_H

#include "image_subtitle.h"
#include <dcp/subtitle_string.h>

class PlayerSubtitles
{
public:
	PlayerSubtitles (DCPTime f, DCPTime t)
		: from (f)
		, to (t)
	{}
	
	DCPTime from;
	DCPTime to;

	/** ImageSubtitles, with their rectangles transformed as specified by their content */
	std::list<ImageSubtitle> image;
	std::list<dcp::SubtitleString> text; 
};

#endif

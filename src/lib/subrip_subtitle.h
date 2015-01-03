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

#ifndef DCPOMATIC_SUBRIP_SUBTITLE_H
#define DCPOMATIC_SUBRIP_SUBTITLE_H

#include "types.h"
#include "dcpomatic_time.h"
#include <dcp/types.h>
#include <boost/optional.hpp>

struct SubRipSubtitlePiece
{
	SubRipSubtitlePiece ()
		: bold (false)
		, italic (false)
		, underline (false)
	{}
	
	std::string text;
	bool bold;
	bool italic;
	bool underline;
	dcp::Colour color;
};

struct SubRipSubtitle
{
	ContentTimePeriod period;
	boost::optional<int> x1;
	boost::optional<int> x2;
	boost::optional<int> y1;
	boost::optional<int> y2;
	std::list<SubRipSubtitlePiece> pieces;
};

#endif

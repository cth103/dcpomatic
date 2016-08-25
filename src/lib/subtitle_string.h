/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SUBTITLE_STRING_H
#define DCPOMATIC_SUBTITLE_STRING_H

#include <dcp/subtitle_string.h>

/** A wrapper for SubtitleString which allows us to include settings that are not
 *  applicable to true DCP subtitles.  For example, we can set outline width for burn-in
 *  but this cannot be specified in DCP XML.
 */
class SubtitleString : public dcp::SubtitleString
{
public:
	SubtitleString (dcp::SubtitleString dcp_)
		: dcp::SubtitleString (dcp_)
		, outline_width (2)
	{}

	SubtitleString (dcp::SubtitleString dcp_, int outline_width_)
		: dcp::SubtitleString (dcp_)
		, outline_width (outline_width_)
	{}

	int outline_width;
};

#endif

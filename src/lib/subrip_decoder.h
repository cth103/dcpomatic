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

#ifndef DCPOMATIC_SUBRIP_DECODER_H
#define DCPOMATIC_SUBRIP_DECODER_H

#include "subtitle_decoder.h"
#include "subrip.h"

class SubRipContent;

class SubRipDecoder : public SubtitleDecoder, public SubRip
{
public:
	SubRipDecoder (boost::shared_ptr<const SubRipContent>);

protected:
	void seek (ContentTime time, bool accurate);
	bool pass (PassReason);

private:
	std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod, bool starting) const;
	std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod, bool starting) const;
	
	size_t _next;
};

#endif

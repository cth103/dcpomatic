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

#include "subtitle_decoder.h"
#include "dcp_subtitle.h"

class DCPSubtitleContent;

class DCPSubtitleDecoder : public SubtitleDecoder, public DCPSubtitle
{
public:
	DCPSubtitleDecoder (boost::shared_ptr<const DCPSubtitleContent>);

protected:
	bool pass (PassReason);
	void seek (ContentTime time, bool accurate);

private:
	std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod, bool starting) const;
	std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod, bool starting) const;

	std::list<dcp::SubtitleString> _subtitles;
	std::list<dcp::SubtitleString>::const_iterator _next;
};

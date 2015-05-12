/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "dcpomatic_time.h"
#include "ffmpeg_stream.h"

class FFmpegSubtitleStream : public FFmpegStream
{
public:
	FFmpegSubtitleStream (std::string n, int i)
		: FFmpegStream (n, i)
	{}
	
	FFmpegSubtitleStream (cxml::ConstNodePtr);

	void as_xml (xmlpp::Node *) const;

	void add_subtitle (ContentTimePeriod period);
	std::list<ContentTimePeriod> subtitles_during (ContentTimePeriod period, bool starting) const;
	ContentTime find_subtitle_to (ContentTime from) const;

private:
	std::map<ContentTime, ContentTime> _subtitles;
};


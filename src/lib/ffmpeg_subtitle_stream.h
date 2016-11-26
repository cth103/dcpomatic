/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcpomatic_time.h"
#include "rgba.h"
#include "ffmpeg_stream.h"
#include <map>

class FFmpegSubtitleStream : public FFmpegStream
{
public:
	FFmpegSubtitleStream (std::string n, int i)
		: FFmpegStream (n, i)
	{}

	FFmpegSubtitleStream (cxml::ConstNodePtr, int version);

	void as_xml (xmlpp::Node *) const;

	void add_image_subtitle (std::string id, ContentTimePeriod period);
	void add_text_subtitle (std::string id, ContentTimePeriod period);
	void set_subtitle_to (std::string id, ContentTime to);
	ContentTime find_subtitle_to (std::string id) const;
	bool unknown_to (std::string id) const;
	void add_offset (ContentTime offset);
	void set_colour (RGBA from, RGBA to);
	std::map<RGBA, RGBA> colours () const;

	bool has_text () const;
	bool has_image () const;

private:

	typedef std::map<std::string, ContentTimePeriod> PeriodMap;

	void as_xml (xmlpp::Node *, PeriodMap const & subs, std::string node) const;

	PeriodMap _image_subtitles;
	PeriodMap _text_subtitles;
	std::map<RGBA, RGBA> _colours;
};

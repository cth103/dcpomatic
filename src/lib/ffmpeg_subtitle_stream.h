/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include "rgba.h"
#include "ffmpeg_stream.h"

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
	std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod period, bool starting) const;
	std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod period, bool starting) const;
	ContentTime find_subtitle_to (std::string id) const;
	void add_offset (ContentTime offset);
	void set_colour (RGBA from, RGBA to);

	bool has_image_subtitles () const {
		return !_image_subtitles.empty ();
	}
	bool has_text_subtitles () const {
		return !_text_subtitles.empty ();
	}
	std::map<RGBA, RGBA> colours () const;

private:

	typedef std::map<std::string, ContentTimePeriod> PeriodMap;

	void as_xml (xmlpp::Node *, PeriodMap const & subs, std::string node) const;
	std::list<ContentTimePeriod> subtitles_during (ContentTimePeriod period, bool starting, PeriodMap const & subs) const;

	PeriodMap _image_subtitles;
	PeriodMap _text_subtitles;
	std::map<RGBA, RGBA> _colours;
};

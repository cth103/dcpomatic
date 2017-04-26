/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

	FFmpegSubtitleStream (cxml::ConstNodePtr node, int version);

	void as_xml (xmlpp::Node *) const;

	void set_colour (RGBA from, RGBA to);
	std::map<RGBA, RGBA> colours () const;

private:
	std::map<RGBA, RGBA> _colours;
};

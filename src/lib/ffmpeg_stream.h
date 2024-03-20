/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FFMPEG_STREAM_H
#define DCPOMATIC_FFMPEG_STREAM_H

#include <libcxml/cxml.h>
#include <boost/lexical_cast.hpp>

struct AVFormatContext;
struct AVStream;

class FFmpegStream
{
public:
	FFmpegStream (std::string n, int i)
		: name (n)
		, _id (i)
	{}

	explicit FFmpegStream (cxml::ConstNodePtr);

	void as_xml(xmlpp::Element*) const;

	/** @param c An AVFormatContext.
	 *  @param index A stream index within the AVFormatContext.
	 *  @return true if this FFmpegStream uses the given stream index.
	 */
	bool uses_index (AVFormatContext const * c, int index) const;
	AVStream* stream (AVFormatContext const * c) const;

	std::string technical_summary () const {
		return "id " + boost::lexical_cast<std::string> (_id);
	}

	std::string identifier () const {
		return boost::lexical_cast<std::string> (_id);
	}

	int id () const {
		return _id;
	}

	int index (AVFormatContext const * c) const;

	std::string name;

	friend bool operator== (FFmpegStream const & a, FFmpegStream const & b);
	friend bool operator!= (FFmpegStream const & a, FFmpegStream const & b);

private:
	int _id;
};

#endif

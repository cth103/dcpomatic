/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TEXT_RING_BUFFERS_H
#define DCPOMATIC_TEXT_RING_BUFFERS_H

#include "player_text.h"
#include "dcp_text_track.h"
#include <boost/thread.hpp>
#include <utility>

class TextRingBuffers
{
public:
	void put (PlayerText text, DCPTextTrack track, DCPTimePeriod period);

	struct Data {
		Data (PlayerText text_, DCPTextTrack track_, DCPTimePeriod period_)
			: text (text_)
			, track (track_)
			, period (period_)
		{}

		PlayerText text;
		DCPTextTrack track;
		DCPTimePeriod period;
	};

	boost::optional<Data> get ();
	void clear ();

private:
	boost::mutex _mutex;

	std::list<Data> _data;
};

#endif

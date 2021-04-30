/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "text_ring_buffers.h"


using std::pair;
using boost::optional;
using namespace dcpomatic;


void
TextRingBuffers::put (PlayerText text, DCPTextTrack track, DCPTimePeriod period)
{
	boost::mutex::scoped_lock lm (_mutex);
	_data.push_back (Data(text, track, period));
}


optional<TextRingBuffers::Data>
TextRingBuffers::get ()
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_data.empty()) {
		return {};
	}

	auto r = _data.front();
	_data.pop_front();
	return r;
}


void
TextRingBuffers::clear ()
{
	_data.clear ();
}

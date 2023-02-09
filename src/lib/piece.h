/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_PIECE_H
#define DCPOMATIC_PIECE_H


#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include <vector>


class Content;
class Decoder;


class Piece
{
public:
	Piece (std::shared_ptr<Content> c, std::shared_ptr<Decoder> d, FrameRateChange f)
		: content (c)
		, decoder (d)
		, frc (f)
		, done (false)
	{}

	std::shared_ptr<Content> content;
	std::shared_ptr<Decoder> decoder;
	std::vector<dcpomatic::DCPTimePeriod> ignore_video;
	std::vector<dcpomatic::DCPTimePeriod> ignore_atmos;
	FrameRateChange frc;
	bool done;
};


#endif

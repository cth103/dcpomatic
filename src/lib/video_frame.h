/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_VIDEO_FRAME_H
#define DCPOMATIC_VIDEO_FRAME_H

#include "types.h"

class VideoFrame
{
public:
	VideoFrame ()
		: _index (0)
		, _eyes (EYES_BOTH)
	{}

	explicit VideoFrame (Frame i)
		: _index (i)
		, _eyes (EYES_BOTH)
	{}

	VideoFrame (Frame i, Eyes e)
		: _index (i)
		, _eyes (e)
	{}

	Frame index () const {
		return _index;
	}

	Eyes eyes () const {
		return _eyes;
	}

	VideoFrame& operator++ ();

private:
	Frame _index;
	Eyes _eyes;
};

extern bool operator== (VideoFrame const & a, VideoFrame const & b);
extern bool operator!= (VideoFrame const & a, VideoFrame const & b);
extern bool operator> (VideoFrame const & a, VideoFrame const & b);

#endif

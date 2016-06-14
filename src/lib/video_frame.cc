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

#include "video_frame.h"
#include "dcpomatic_assert.h"

VideoFrame &
VideoFrame::operator++ ()
{
	if (_eyes == EYES_BOTH) {
		++_index;
	} else if (_eyes == EYES_LEFT) {
		_eyes = EYES_RIGHT;
	} else {
		_eyes = EYES_LEFT;
		++_index;
	}

	return *this;
}

bool
operator== (VideoFrame const & a, VideoFrame const & b)
{
	return a.index() == b.index() && a.eyes() == b.eyes();
}

bool
operator!= (VideoFrame const & a, VideoFrame const & b)
{
	return !(a == b);
}

bool
operator> (VideoFrame const & a, VideoFrame const & b)
{
	if (a.index() != b.index()) {
		return a.index() > b.index();
	}

	/* indexes are the same */

	if (a.eyes() == b.eyes()) {
		return false;
	}

	/* eyes are not the same */

	if (a.eyes() == EYES_LEFT && b.eyes() == EYES_RIGHT) {
		return false;
	}

	if (a.eyes() == EYES_RIGHT && b.eyes() == EYES_LEFT) {
		return true;
	}

	/* should never get here; we are comparing 2D with 3D */

	DCPOMATIC_ASSERT (false);
}

/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "frame_interval_checker.h"

using namespace dcpomatic;

int const FrameIntervalChecker::_frames = 16;

void
FrameIntervalChecker::feed (ContentTime time, double frame_rate)
{
	/* The caller isn't meant to feed too much data before
	   calling guess() and destroying the FrameIntervalChecker.
	 */
	DCPOMATIC_ASSERT (_intervals.size() < _frames);

	if (_last) {
		_intervals.push_back ((time - *_last).seconds() * frame_rate);
	}

	_last = time;
}

FrameIntervalChecker::Guess
FrameIntervalChecker::guess () const
{
	if (_intervals.size() < _frames) {
		/* How soon can you land?
		 * I can't tell.
		 * You can tell me, I'm a doctor.
		 * Nom I mean I'm just not sure.
		 * Can't you take a guess?
		 * Well, not for another two hours.
		 * You can't take a guess for another two hours?
		 */
		return AGAIN;
	}

	int near_1 = 0;
	for (auto i: _intervals) {
		if (i > 0.5) {
			++near_1;
		}
	}

	return (near_1 < 3 * _frames / 4) ? PROBABLY_3D : PROBABLY_NOT_3D;
}


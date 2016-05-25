/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FRAME_RATE_CHANGE_H
#define DCPOMATIC_FRAME_RATE_CHANGE_H

#include <string>

struct FrameRateChange
{
	FrameRateChange (double, int);

	/** @return factor by which to multiply a source frame rate
	    to get the effective rate after any skip or repeat has happened.
	*/
	double factor () const {
		if (skip) {
			return 0.5;
		}

		return repeat;
	}

	double source;
	int dcp;

	/** true to skip every other frame */
	bool skip;
	/** number of times to use each frame (e.g. 1 is normal, 2 means repeat each frame once, and so on) */
	int repeat;
	/** true if this DCP will run its video faster or slower than the source
	 *  without taking into account `repeat' nor `skip'.
	 *  (e.g. change_speed will be true if
	 *	    source is 29.97fps, DCP is 30fps
	 *	    source is 14.50fps, DCP is 30fps
	 *  but not if
	 *	    source is 15.00fps, DCP is 30fps
	 *	    source is 12.50fps, DCP is 25fps)
	 */
	bool change_speed;

	/** Amount by which the video is being sped-up in the DCP; e.g. for a
	 *  24fps source in a 25fps DCP this would be 25/24.
	 */
	double speed_up;

	std::string description () const;
};

#endif

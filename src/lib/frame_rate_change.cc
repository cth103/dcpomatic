/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <cmath>
#include "frame_rate_change.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;

static bool
about_equal (float a, float b)
{
	/* A film of F seconds at f FPS will be Ff frames;
	   Consider some delta FPS d, so if we run the same
	   film at (f + d) FPS it will last F(f + d) seconds.

	   Hence the difference in length over the length of the film will
	   be F(f + d) - Ff frames
	    = Ff + Fd - Ff frames
	    = Fd frames
	    = Fd/f seconds

	   So if we accept a difference of 1 frame, ie 1/f seconds, we can
	   say that

	   1/f = Fd/f
	ie 1 = Fd
	ie d = 1/F

	   So for a 3hr film, ie F = 3 * 60 * 60 = 10800, the acceptable
	   FPS error is 1/F ~= 0.0001 ~= 10-e4
	*/

	return (fabs (a - b) < 1e-4);
}


FrameRateChange::FrameRateChange (float source_, int dcp_)
	: source (source_)
	, dcp (dcp_)
	, skip (false)
	, repeat (1)
	, change_speed (false)
{
	if (fabs (source / 2.0 - dcp) < fabs (source - dcp)) {
		/* The difference between source and DCP frame rate will be lower
		   (i.e. better) if we skip.
		*/
		skip = true;
	} else if (fabs (source * 2 - dcp) < fabs (source - dcp)) {
		/* The difference between source and DCP frame rate would be better
		   if we repeated each frame once; it may be better still if we
		   repeated more than once.  Work out the required repeat.
		*/
		repeat = round (dcp / source);
	}

	speed_up = dcp / (source * factor());
	change_speed = !about_equal (speed_up, 1.0);
}

string
FrameRateChange::description () const
{
	string description;

	if (!skip && repeat == 1 && !change_speed) {
		description = _("Content and DCP have the same rate.\n");
	} else {
		if (skip) {
			description = _("DCP will use every other frame of the content.\n");
		} else if (repeat == 2) {
			description = _("Each content frame will be doubled in the DCP.\n");
		} else if (repeat > 2) {
			description = String::compose (_("Each content frame will be repeated %1 more times in the DCP.\n"), repeat - 1);
		}

		if (change_speed) {
			float const pc = dcp * 100 / (source * factor());
			description += String::compose (_("DCP will run at %1%% of the content speed.\n"), pc);
		}
	}

	return description;
}

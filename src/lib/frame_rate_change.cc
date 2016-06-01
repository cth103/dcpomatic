/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "frame_rate_change.h"
#include "types.h"
#include "compose.hpp"
#include <cmath>

#include "i18n.h"

using std::string;

static bool
about_equal (double a, double b)
{
	return (fabs (a - b) < VIDEO_FRAME_RATE_EPSILON);
}


FrameRateChange::FrameRateChange (double source_, int dcp_)
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
			double const pc = dcp * 100 / (source * factor());
			description += String::compose (_("DCP will run at %1%% of the content speed.\n"), pc);
		}
	}

	return description;
}

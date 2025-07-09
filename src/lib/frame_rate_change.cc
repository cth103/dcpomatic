/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "content.h"
#include "film.h"
#include "frame_rate_change.h"
#include <cmath>

#include "i18n.h"


using std::shared_ptr;
using std::string;


FrameRateChange::FrameRateChange ()
{

}


FrameRateChange::FrameRateChange (double source_, int dcp_)
{
	source = source_;
	dcp = dcp_;

	if (fabs(source / 2.0 - dcp) < fabs(source - dcp)) {
		/* The difference between source and DCP frame rate will be lower
		   (i.e. better) if we skip.
		*/
		skip = true;
	} else if (fabs(source * 2 - dcp) < fabs(source - dcp)) {
		/* The difference between source and DCP frame rate would be better
		   if we repeated each frame once; it may be better still if we
		   repeated more than once.  Work out the required repeat.
		*/
		repeat = round (dcp / source);
	}

	speed_up = dcp / (source * factor());

	auto about_equal = [](double a, double b) {
		return (fabs (a - b) < VIDEO_FRAME_RATE_EPSILON);
	};

	change_speed = !about_equal (speed_up, 1.0);
}


FrameRateChange::FrameRateChange (shared_ptr<const Film> film, shared_ptr<const Content> content)
	: FrameRateChange (content->active_video_frame_rate(film), film->video_frame_rate())
{

}


FrameRateChange::FrameRateChange (shared_ptr<const Film> film, Content const * content)
	: FrameRateChange (content->active_video_frame_rate(film), film->video_frame_rate())
{

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
			description = fmt::format(_("Each content frame will be repeated {} more times in the DCP.\n"), repeat - 1);
		}

		if (change_speed) {
			double const pc = dcp * 100 / (source * factor());
			char buffer[256];
			snprintf (buffer, sizeof(buffer), _("DCP will run at %.1f%% of the content speed.\n"), pc);
			description += buffer;
		}
	}

	return description;
}

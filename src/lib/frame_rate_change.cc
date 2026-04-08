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


#include "content.h"
#include "film.h"
#include "frame_rate_change.h"
#include <cmath>

#include "i18n.h"


using std::shared_ptr;
using std::string;


FrameRateChange::FrameRateChange()
{

}


FrameRateChange::FrameRateChange(double source, int dcp)
{
	_source = source;
	_dcp = dcp;

	if (fabs(_source / 2.0 - _dcp) < fabs(_source - _dcp)) {
		/* The difference between source and DCP frame rate will be lower
		   (i.e. better) if we skip.
		*/
		_skip = true;
	} else if (fabs(_source * 2 - _dcp) < fabs(_source - _dcp)) {
		/* The difference between source and DCP frame rate would be better
		   if we repeated each frame once; it may be better still if we
		   repeated more than once.  Work out the required repeat.
		*/
		_repeat = round(_dcp / _source);
	}

	_speed_up = _dcp / (_source * factor());

	auto about_equal = [](double a, double b) {
		return fabs(a - b) < VIDEO_FRAME_RATE_EPSILON;
	};

	_change_speed = !about_equal(_speed_up, 1.0);
}


FrameRateChange::FrameRateChange(shared_ptr<const Film> film, shared_ptr<const Content> content)
	: FrameRateChange(content->active_video_frame_rate(film), film->video_frame_rate())
{

}


FrameRateChange::FrameRateChange(shared_ptr<const Film> film, Content const * content)
	: FrameRateChange(content->active_video_frame_rate(film), film->video_frame_rate())
{

}


string
FrameRateChange::description() const
{
	string description;

	if (!_skip && _repeat == 1 && !_change_speed) {
		description = _("Content and DCP have the same rate.\n");
	} else {
		if (_skip) {
			description = _("DCP will use every other frame of the content.\n");
		} else if (_repeat == 2) {
			description = _("Each content frame will be doubled in the DCP.\n");
		} else if (_repeat > 2) {
			description = fmt::format(_("Each content frame will be repeated {} more times in the DCP.\n"), _repeat - 1);
		}

		if (_change_speed) {
			double const pc = _dcp * 100 / (_source * factor());
			char buffer[256];
			snprintf(buffer, sizeof(buffer), _("DCP will run at %.1f%% of the content speed.\n"), pc);
			description += buffer;
		}
	}

	return description;
}

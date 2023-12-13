/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_TIMELINE_VIEW_H
#define DCPOMATIC_TIMELINE_VIEW_H


#include "lib/rect.h"
#include "lib/dcpomatic_time.h"


class wxGraphicsContext;


/** @class ContentTimelineView
 *  @brief Parent class for components of the content timeline (e.g. a piece of content or an axis).
 */
template <class Timeline>
class TimelineView
{
public:
	explicit TimelineView(Timeline& timeline)
		: _timeline(timeline)
	{}

	virtual ~TimelineView () = default;

	TimelineView(TimelineView const&) = delete;
	TimelineView& operator=(TimelineView const&) = delete;

	void force_redraw()
	{
		_timeline.force_redraw(_last_paint_bbox.extended(4));
		_timeline.force_redraw(bbox().extended(4));
	}

	virtual dcpomatic::Rect<int> bbox() const = 0;

protected:
	int time_x(dcpomatic::DCPTime t) const
	{
		return t.seconds() * _timeline.pixels_per_second().get_value_or(0);
	}

	Timeline& _timeline;
	dcpomatic::Rect<int> _last_paint_bbox;
};


#endif


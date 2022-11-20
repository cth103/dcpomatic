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


#ifndef DCPOMATIC_TIMELINE_VIEW_H
#define DCPOMATIC_TIMELINE_VIEW_H


#include "lib/rect.h"
#include "lib/dcpomatic_time.h"


class wxGraphicsContext;
class Timeline;


/** @class TimelineView
 *  @brief Parent class for components of the timeline (e.g. a piece of content or an axis).
 */
class TimelineView
{
public:
	explicit TimelineView (Timeline& t);
	virtual ~TimelineView () {}

	TimelineView (TimelineView const&) = delete;
	TimelineView& operator= (TimelineView const&) = delete;

	void paint (wxGraphicsContext* g, std::list<dcpomatic::Rect<int>> overlaps);
	void force_redraw ();

	virtual dcpomatic::Rect<int> bbox () const = 0;

protected:
	virtual void do_paint (wxGraphicsContext *, std::list<dcpomatic::Rect<int>> overlaps) = 0;

	int time_x (dcpomatic::DCPTime t) const;
	int y_pos(int t) const;

	Timeline& _timeline;

private:
	dcpomatic::Rect<int> _last_paint_bbox;
};


typedef std::vector<std::shared_ptr<TimelineView>> TimelineViewList;


#endif

/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "timeline_view.h"
#include "timeline.h"

using std::list;

/** @class TimelineView
 *  @brief Parent class for components of the timeline (e.g. a piece of content or an axis).
 */
TimelineView::TimelineView (Timeline& t)
	: _timeline (t)
{

}

void
TimelineView::paint (wxGraphicsContext* g, list<dcpomatic::Rect<int> > overlaps)
{
	_last_paint_bbox = bbox ();
	do_paint (g, overlaps);
}

void
TimelineView::force_redraw ()
{
	_timeline.force_redraw (_last_paint_bbox.extended (4));
	_timeline.force_redraw (bbox().extended (4));
}

int
TimelineView::time_x (DCPTime t) const
{
	return _timeline.tracks_position().x + t.seconds() * _timeline.pixels_per_second().get_value_or (0);
}

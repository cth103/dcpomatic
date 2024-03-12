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


#include "content_timeline.h"
#include "content_timeline_view.h"


using std::list;
using namespace dcpomatic;


ContentTimelineView::ContentTimelineView(ContentTimeline& timeline)
	: TimelineView(timeline)
{

}

int
ContentTimelineView::y_pos(int t) const
{
	return t * _timeline.pixels_per_track() + _timeline.tracks_y_offset();
}



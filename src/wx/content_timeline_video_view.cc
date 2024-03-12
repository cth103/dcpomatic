/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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


#include "colours.h"
#include "content_timeline_video_view.h"
#include "lib/image_content.h"
#include "lib/video_content.h"


using std::dynamic_pointer_cast;
using std::shared_ptr;


ContentTimelineVideoView::ContentTimelineVideoView(ContentTimeline& tl, shared_ptr<Content> c)
	: TimelineContentView (tl, c)
{

}

wxColour
ContentTimelineVideoView::background_colour() const
{
	if (!active()) {
		return wxColour (210, 210, 210, 128);
	}

	return VIDEO_CONTENT_COLOUR;
}

wxColour
ContentTimelineVideoView::foreground_colour() const
{
	if (!active()) {
		return wxColour (180, 180, 180, 128);
	}

	return wxColour (0, 0, 0, 255);
}

bool
ContentTimelineVideoView::active() const
{
	auto c = _content.lock();
	DCPOMATIC_ASSERT (c);
	return c->video && c->video->use();
}

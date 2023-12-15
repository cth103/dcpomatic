/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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


#include "content_timeline_text_view.h"
#include "lib/text_content.h"
#include "lib/content.h"


using std::shared_ptr;


ContentTimelineTextView::ContentTimelineTextView(ContentTimeline& tl, shared_ptr<Content> c, shared_ptr<TextContent> caption)
	: TimelineContentView (tl, c)
	, _caption (caption)
{

}

wxColour
ContentTimelineTextView::background_colour() const
{
	if (!active ()) {
		return wxColour (210, 210, 210, 128);
	}

	return wxColour (163, 255, 154, 255);
}

wxColour
ContentTimelineTextView::foreground_colour() const
{
	if (!active ()) {
		return wxColour (180, 180, 180, 128);
	}

	return wxColour (0, 0, 0, 255);
}

bool
ContentTimelineTextView::active() const
{
	return _caption->use();
}

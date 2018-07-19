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

#include "timeline_text_content_view.h"
#include "lib/caption_content.h"
#include "lib/content.h"

using boost::shared_ptr;

TimelineTextContentView::TimelineTextContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineContentView (tl, c)
{

}

wxColour
TimelineTextContentView::background_colour () const
{
	if (!active ()) {
		return wxColour (210, 210, 210, 128);
	}

	return wxColour (163, 255, 154, 255);
}

wxColour
TimelineTextContentView::foreground_colour () const
{
	if (!active ()) {
		return wxColour (180, 180, 180, 128);
	}

	return wxColour (0, 0, 0, 255);
}

bool
TimelineTextContentView::active () const
{
	shared_ptr<Content> c = _content.lock ();
	DCPOMATIC_ASSERT (c);
	return c->caption && c->caption->use();
}

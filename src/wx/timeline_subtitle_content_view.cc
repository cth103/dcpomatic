/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "timeline_subtitle_content_view.h"
#include "lib/subtitle_content.h"

using boost::shared_ptr;

TimelineSubtitleContentView::TimelineSubtitleContentView (Timeline& tl, shared_ptr<SubtitleContent> c)
	: TimelineContentView (tl, c)
	, _subtitle_content (c)
{

}

wxString
TimelineSubtitleContentView::type () const
{
	return _("subtitles");
}

wxColour
TimelineSubtitleContentView::background_colour () const
{
	shared_ptr<SubtitleContent> sc = _subtitle_content.lock ();
	if (!sc || !sc->use_subtitles ()) {
		return wxColour (210, 210, 210, 128);
	}
	
	return wxColour (163, 255, 154, 255);
}

wxColour
TimelineSubtitleContentView::foreground_colour () const
{
	shared_ptr<SubtitleContent> sc = _subtitle_content.lock ();
	if (!sc || !sc->use_subtitles ()) {
		return wxColour (180, 180, 180, 128);
	}
	
	return wxColour (0, 0, 0, 255);
}

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

#include "timeline_audio_content_view.h"

using boost::shared_ptr;

/** @class TimelineAudioContentView
 *  @brief Timeline view for AudioContent.
 */

TimelineAudioContentView::TimelineAudioContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineContentView (tl, c)
{

}

wxString
TimelineAudioContentView::type () const
{
	return _("audio");
}

wxColour
TimelineAudioContentView::background_colour () const
{
	return wxColour (149, 121, 232, 255);
}

wxColour
TimelineAudioContentView::foreground_colour () const
{
	return wxColour (0, 0, 0, 255);
}

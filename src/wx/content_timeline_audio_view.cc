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


#include "colours.h"
#include "content_timeline_audio_view.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/util.h"


using std::dynamic_pointer_cast;
using std::list;
using std::shared_ptr;


/** @class ContentTimelineAudioView
 *  @brief Content timeline view for AudioContent.
 */


ContentTimelineAudioView::ContentTimelineAudioView(ContentTimeline& tl, shared_ptr<Content> c)
	: TimelineContentView (tl, c)
{

}

wxColour
ContentTimelineAudioView::background_colour () const
{
	return AUDIO_CONTENT_COLOUR;
}

wxColour
ContentTimelineAudioView::foreground_colour () const
{
	return wxColour (0, 0, 0, 255);
}

wxString
ContentTimelineAudioView::label () const
{
	wxString s = TimelineContentView::label ();
	shared_ptr<AudioContent> ac = content()->audio;
	DCPOMATIC_ASSERT (ac);

	if (ac->gain() > 0.01) {
		s += wxString::Format (" +%.1fdB", ac->gain());
	} else if (ac->gain() < -0.01) {
		s += wxString::Format (" %.1fdB", ac->gain());
	}

	if (ac->delay() > 0) {
		s += wxString::Format (_(" delayed by %dms"), ac->delay());
	} else if (ac->delay() < 0) {
		s += wxString::Format (_(" advanced by %dms"), -ac->delay());
	}

	list<int> mapped = ac->mapping().mapped_output_channels();
	if (!mapped.empty ()) {
		s += wxString::FromUTF8(" → ");
		for (auto i: mapped) {
			s += std_to_wx(short_audio_channel_name(i)) + ", ";
		}
		s = s.Left(s.Length() - 2);
	}

	return s;
}

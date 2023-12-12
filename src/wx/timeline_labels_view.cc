/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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
#include "timeline_labels_view.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::list;
using std::max;
using std::min;


TimelineLabelsView::TimelineLabelsView(ContentTimeline& tl)
	: TimelineView (tl)
{
	wxString labels[] = {
		_("Video"),
		_("Audio"),
		_("Subtitles/captions"),
		_("Atmos")
	};

        wxClientDC dc (&_timeline);
	for (int i = 0; i < 3; ++i) {
		auto size = dc.GetTextExtent (labels[i]);
		_width = max (_width, size.GetWidth());
	}

	_width += 24;
}


dcpomatic::Rect<int>
TimelineLabelsView::bbox () const
{
	return dcpomatic::Rect<int> (0, 0, _width, _timeline.tracks() * _timeline.pixels_per_track());
}


void
TimelineLabelsView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int>>)
{
	int const h = _timeline.pixels_per_track ();
	wxColour const colour = gui_is_dark() ? *wxWHITE : *wxBLACK;
	gc->SetFont(gc->CreateFont(wxNORMAL_FONT->Bold(), colour));

	int fy = 0;
	if (_video_tracks) {
		int ty = fy + _video_tracks * h;
		gc->DrawText (_("Video"), 0, (ty + fy) / 2 - 8);
		fy = ty;
	}

	if (_text_tracks) {
		int ty = fy + _text_tracks * h;
		gc->DrawText (_("Subtitles/captions"), 0, (ty + fy) / 2 - 8);
		fy = ty;
	}

	if (_atmos) {
		int ty = fy + h;
		gc->DrawText (_("Atmos"), 0, (ty + fy) / 2 - 8);
		fy = ty;
	}

	if (_audio_tracks) {
		int ty = _timeline.tracks() * h;
		gc->DrawText (_("Audio"), 0, (ty + fy) / 2 - 8);
	}
}


void
TimelineLabelsView::set_video_tracks (int n)
{
	_video_tracks = n;
}


void
TimelineLabelsView::set_audio_tracks (int n)
{
	_audio_tracks = n;
}


void
TimelineLabelsView::set_text_tracks (int n)
{
	_text_tracks = n;
}


void
TimelineLabelsView::set_atmos (bool s)
{
	_atmos = s;
}

/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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


#include "timeline_labels_view.h"
#include "timeline.h"
#include <wx/wx.h>
#include <wx/graphics.h>

using std::list;
using std::min;
using std::max;

TimelineLabelsView::TimelineLabelsView (Timeline& tl)
	: TimelineView (tl)
	, _threed (true)
	, _subtitle_tracks (0)
	, _atmos (true)
{
	wxString labels[] = {
		_("Video"),
		_("Audio"),
		_("Subtitles"),
		_("Atmos")
	};

	_width = 0;

        wxClientDC dc (&_timeline);
	for (int i = 0; i < 3; ++i) {
		wxSize size = dc.GetTextExtent (labels[i]);
		_width = max (_width, size.GetWidth());
	}

	_width += 16;
}

dcpomatic::Rect<int>
TimelineLabelsView::bbox () const
{
	return dcpomatic::Rect<int> (0, 0, _width, _timeline.tracks() * _timeline.pixels_per_track());
}

void
TimelineLabelsView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int> >)
{
	int const h = _timeline.pixels_per_track ();
	gc->SetFont (gc->CreateFont(wxNORMAL_FONT->Bold(), wxColour (0, 0, 0)));

	int fy = 0;
	int ty = _threed ? 2 * h : h;
	gc->DrawText (_("Video"), 0, (ty + fy) / 2 - 8);
	fy = ty;

	if (_subtitle_tracks) {
		ty = fy + _subtitle_tracks * h;
		gc->DrawText (_("Subtitles"), 0, (ty + fy) / 2 - 8);
		fy = ty;
	}

	if (_atmos) {
		ty = fy + h;
		gc->DrawText (_("Atmos"), 0, (ty + fy) / 2 - 8);
		fy = ty;
	}

	ty = _timeline.tracks() * h;
	gc->DrawText (_("Audio"), 0, (ty + fy) / 2 - 8);
}

void
TimelineLabelsView::set_3d (bool s)
{
	_threed = s;
}

void
TimelineLabelsView::set_subtitle_tracks (int n)
{
	_subtitle_tracks = n;
}

void
TimelineLabelsView::set_atmos (bool s)
{
	_atmos = s;
}

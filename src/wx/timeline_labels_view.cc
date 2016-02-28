/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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


#include "timeline_labels_view.h"
#include "timeline.h"
#include <wx/wx.h>
#include <wx/graphics.h>

using std::list;
using std::min;
using std::max;

TimelineLabelsView::TimelineLabelsView (Timeline& tl)
	: TimelineView (tl)
{
	wxString labels[] = {
		_("Video"),
		_("Audio"),
		_("Subtitles")
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
	return dcpomatic::Rect<int> (0, 0, _width, _timeline.tracks() * _timeline.track_height());
}

void
TimelineLabelsView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int> >)
{
	int const h = _timeline.track_height ();
	gc->SetFont (gc->CreateFont(wxNORMAL_FONT->Bold(), wxColour (0, 0, 0)));
	gc->DrawText (_("Video"), 0, h / 2);
	gc->DrawText (_("Subtitles"), 0, 3 * h / 2);
	gc->DrawText (_("Audio"), 0, h + max (_timeline.tracks(), 2) * h / 2);
}

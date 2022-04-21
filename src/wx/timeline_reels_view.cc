/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "timeline.h"
#include "timeline_reels_view.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/graphics.h>
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


using std::min;
using std::list;
using namespace dcpomatic;


TimelineReelsView::TimelineReelsView (Timeline& tl, int y)
	: TimelineView (tl)
	, _y (y)
{

}


dcpomatic::Rect<int>
TimelineReelsView::bbox () const
{
	return dcpomatic::Rect<int> (0, _y - 4, _timeline.width(), 24);
}


void
TimelineReelsView::set_y (int y)
{
	_y = y;
	force_redraw ();
}


void
TimelineReelsView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int>>)
{
	if (!_timeline.pixels_per_second()) {
		return;
	}

	double const pps = _timeline.pixels_per_second().get ();

	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 255), 1, wxPENSTYLE_SOLID));

	auto path = gc->CreatePath ();
	path.MoveToPoint (time_x (DCPTime (0)), _y);
	path.AddLineToPoint (time_x (_timeline.film()->length()), _y);
	gc->StrokePath (path);

	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT, wxColour (0, 0, 255)));

	int reel = 1;
	for (auto i: _timeline.film()->reels()) {
		int const size = min (8.0, i.duration().seconds() * pps / 2);

		auto path = gc->CreatePath ();
		path.MoveToPoint (time_x (i.from) + size, _y + size / 2);
		path.AddLineToPoint (time_x (i.from), _y);
		path.AddLineToPoint (time_x (i.from) + size, _y - size / 2);
		gc->StrokePath (path);

		path = gc->CreatePath ();
		path.MoveToPoint (time_x (i.to) - size, _y + size / 2);
		path.AddLineToPoint (time_x (i.to), _y);
		path.AddLineToPoint (time_x (i.to) - size, _y - size / 2);
		gc->StrokePath (path);

		auto str = wxString::Format (_("Reel %d"), reel++);
		wxDouble str_width;
		wxDouble str_height;
		wxDouble str_descent;
		wxDouble str_leading;
		gc->GetTextExtent (str, &str_width, &str_height, &str_descent, &str_leading);

		int const available_width = time_x(DCPTime(i.to.get())) - time_x(DCPTime(i.from.get()));

		if (available_width > str_width) {
			gc->DrawText (str, time_x(DCPTime(i.from.get())) + (available_width - str_width) / 2, _y + 4);
		}
	}
}

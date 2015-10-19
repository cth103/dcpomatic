/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "timeline_reels_view.h"
#include "timeline.h"
#include <wx/wx.h>
#include <wx/graphics.h>
#include <boost/foreach.hpp>

using std::min;

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
TimelineReelsView::do_paint (wxGraphicsContext* gc)
{
	if (!_timeline.pixels_per_second()) {
		return;
	}

	double const pps = _timeline.pixels_per_second().get ();

	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 255), 1, wxPENSTYLE_SOLID));

	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint (time_x (DCPTime (0)), _y);
	path.AddLineToPoint (time_x (_timeline.film()->length()), _y);
	gc->StrokePath (path);

	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT, wxColour (0, 0, 255)));

	int reel = 1;
	BOOST_FOREACH (DCPTimePeriod i, _timeline.film()->reels()) {
		int const size = min (8.0, i.duration().seconds() * pps / 2);

		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (time_x (i.from) + size, _y + size / 2);
		path.AddLineToPoint (time_x (i.from), _y);
		path.AddLineToPoint (time_x (i.from) + size, _y - size / 2);
		gc->StrokePath (path);

		path = gc->CreatePath ();
		path.MoveToPoint (time_x (i.to) - size, _y + size / 2);
		path.AddLineToPoint (time_x (i.to), _y);
		path.AddLineToPoint (time_x (i.to) - size, _y - size / 2);
		gc->StrokePath (path);

		wxString str = wxString::Format (_("Reel %d"), reel++);
		wxDouble str_width;
		wxDouble str_height;
		wxDouble str_descent;
		wxDouble str_leading;
		gc->GetTextExtent (str, &str_width, &str_height, &str_descent, &str_leading);

		int const tx = time_x (DCPTime((i.from.get() + i.to.get()) / 2));
		gc->DrawText (str, tx - str_width / 2, _y + 4);
	}
}

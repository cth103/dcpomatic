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

#include "timeline_time_axis_view.h"
#include "timeline.h"
#include <wx/wx.h>
#include <wx/graphics.h>

TimelineTimeAxisView::TimelineTimeAxisView (Timeline& tl, int y)
	: TimelineView (tl)
	, _y (y)
{

}
	
dcpomatic::Rect<int>
TimelineTimeAxisView::bbox () const
{
	return dcpomatic::Rect<int> (0, _y - 4, _timeline.width(), 24);
}

void
TimelineTimeAxisView::set_y (int y)
{
	_y = y;
	force_redraw ();
}

void
TimelineTimeAxisView::do_paint (wxGraphicsContext* gc)
{
	if (!_timeline.pixels_per_second()) {
		return;
	}
	
	double const pps = _timeline.pixels_per_second().get ();
	
	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));
	
	double mark_interval = rint (128 / pps);
	if (mark_interval > 5) {
		mark_interval -= int (rint (mark_interval)) % 5;
	}
	if (mark_interval > 10) {
		mark_interval -= int (rint (mark_interval)) % 10;
	}
	if (mark_interval > 60) {
		mark_interval -= int (rint (mark_interval)) % 60;
	}
	if (mark_interval > 3600) {
		mark_interval -= int (rint (mark_interval)) % 3600;
	}
	
	if (mark_interval < 1) {
		mark_interval = 1;
	}
	
	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint (_timeline.x_offset(), _y);
	path.AddLineToPoint (_timeline.width(), _y);
	gc->StrokePath (path);
	
	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT));
	
	/* Time in seconds */
	DCPTime t;
	while ((t.seconds() * pps) < _timeline.width()) {
		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (time_x (t), _y - 4);
		path.AddLineToPoint (time_x (t), _y + 4);
		gc->StrokePath (path);
		
		double tc = t.seconds ();
		int const h = tc / 3600;
		tc -= h * 3600;
		int const m = tc / 60;
		tc -= m * 60;
		int const s = tc;
		
		wxString str = wxString::Format (wxT ("%02d:%02d:%02d"), h, m, s);
		wxDouble str_width;
		wxDouble str_height;
		wxDouble str_descent;
		wxDouble str_leading;
		gc->GetTextExtent (str, &str_width, &str_height, &str_descent, &str_leading);
		
		int const tx = _timeline.x_offset() + t.seconds() * pps;
		if ((tx + str_width) < _timeline.width()) {
			gc->DrawText (str, time_x (t), _y + 16);
		}
		
		t += DCPTime::from_seconds (mark_interval);
	}
}

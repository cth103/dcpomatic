/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include "timeline_time_axis_view.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using std::list;
using namespace dcpomatic;


TimelineTimeAxisView::TimelineTimeAxisView (Timeline& tl, int y)
	: TimelineView (tl)
	, _y (y)
{

}


dcpomatic::Rect<int>
TimelineTimeAxisView::bbox () const
{
	return dcpomatic::Rect<int>(0, y_pos(_y), _timeline.width(), 48);
}


/** @param y y position in tracks (not pixels) */
void
TimelineTimeAxisView::set_y (int y)
{
	_y = y;
	force_redraw ();
}


void
TimelineTimeAxisView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int> >)
{
	if (!_timeline.pixels_per_second()) {
		return;
	}

	double const pps = _timeline.pixels_per_second().get ();

	wxColour const colour = gui_is_dark() ? *wxWHITE : *wxBLACK;
	gc->SetPen(*wxThePenList->FindOrCreatePen(colour, 1, wxPENSTYLE_SOLID));

	double const mark_interval = calculate_mark_interval (rint (128 / pps));

	int y = _y * _timeline.pixels_per_track() + 32;

	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint (0, y);
	path.AddLineToPoint (_timeline.width(), y);
	gc->StrokePath (path);

	gc->SetFont(gc->CreateFont(*wxNORMAL_FONT, colour));

	/* Time in seconds */
	DCPTime t;
	while ((t.seconds() * pps) < _timeline.width()) {
		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (time_x (t), y - 4);
		path.AddLineToPoint (time_x (t), y + 4);
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

		int const tx = t.seconds() * pps;
		if ((tx + str_width) < _timeline.width()) {
			gc->DrawText (str, time_x (t), y + 16);
		}

		t += DCPTime::from_seconds (mark_interval);
	}
}

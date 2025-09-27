/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "dcp_timeline_reel_marker_view.h"
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS


using namespace std;
using namespace dcpomatic;


DCPTimelineReelMarkerView::DCPTimelineReelMarkerView(DCPTimeline& timeline, int y_pos)
	: DCPTimelineView(timeline)
	, _y_pos(y_pos)
{

}


int
DCPTimelineReelMarkerView::x_pos() const
{
	/* Nudge it over slightly so that the full line width is drawn on the left hand side */
	return time_x(_time) + 2;
}


void
DCPTimelineReelMarkerView::do_paint(wxGraphicsContext* gc)
{
	wxColour outline;
	wxColour fill;
	if (_active) {
		outline = gui_is_dark() ? wxColour(190, 190, 190) : wxColour(0, 0, 0);
		fill = gui_is_dark() ? wxColour(190, 0, 0) : wxColour(255, 0, 0);
	} else {
		outline = wxColour(128, 128, 128);
		fill = wxColour(192, 192, 192);
	}

	gc->SetPen(*wxThePenList->FindOrCreatePen(outline, 2, wxPENSTYLE_SOLID));
	gc->SetBrush(*wxTheBrushList->FindOrCreateBrush(fill, wxBRUSHSTYLE_SOLID));

	gc->DrawRectangle(x_pos(), _y_pos, HEAD_SIZE, HEAD_SIZE);

	auto path = gc->CreatePath();
	path.MoveToPoint(x_pos(), _y_pos + HEAD_SIZE + TAIL_LENGTH);
	path.AddLineToPoint(x_pos(), _y_pos);
	gc->StrokePath(path);
	gc->FillPath(path);
}


dcpomatic::Rect<int>
DCPTimelineReelMarkerView::bbox() const
{
	return { x_pos(), _y_pos, HEAD_SIZE, HEAD_SIZE + TAIL_LENGTH };
}


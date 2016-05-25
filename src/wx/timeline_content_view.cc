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

#include "timeline_content_view.h"
#include "timeline.h"
#include "wx_util.h"
#include "lib/content.h"
#include <wx/graphics.h>
#include <boost/foreach.hpp>

using std::list;
using boost::shared_ptr;

TimelineContentView::TimelineContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineView (tl)
	, _content (c)
	, _selected (false)
{
	_content_connection = c->Changed.connect (bind (&TimelineContentView::content_changed, this, _2));
}

dcpomatic::Rect<int>
TimelineContentView::bbox () const
{
	DCPOMATIC_ASSERT (_track);

	shared_ptr<const Film> film = _timeline.film ();
	shared_ptr<const Content> content = _content.lock ();
	if (!film || !content) {
		return dcpomatic::Rect<int> ();
	}

	return dcpomatic::Rect<int> (
		time_x (content->position ()),
		y_pos (_track.get()),
		content->length_after_trim().seconds() * _timeline.pixels_per_second().get_value_or(0),
		_timeline.track_height()
		);
}

void
TimelineContentView::set_selected (bool s)
{
	_selected = s;
	force_redraw ();
}

bool
TimelineContentView::selected () const
{
	return _selected;
}

shared_ptr<Content>
TimelineContentView::content () const
{
	return _content.lock ();
}

void
TimelineContentView::set_track (int t)
{
	_track = t;
}

void
TimelineContentView::unset_track ()
{
	_track = boost::optional<int> ();
}

boost::optional<int>
TimelineContentView::track () const
{
	return _track;
}

void
TimelineContentView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int> > overlaps)
{
	DCPOMATIC_ASSERT (_track);

	shared_ptr<const Film> film = _timeline.film ();
	shared_ptr<const Content> cont = content ();
	if (!film || !cont) {
		return;
	}

	DCPTime const position = cont->position ();
	DCPTime const len = cont->length_after_trim ();

	wxColour selected (background_colour().Red() / 2, background_colour().Green() / 2, background_colour().Blue() / 2);

	gc->SetPen (*wxThePenList->FindOrCreatePen (foreground_colour(), 4, wxPENSTYLE_SOLID));
	if (_selected) {
		gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (selected, wxBRUSHSTYLE_SOLID));
	} else {
		gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (background_colour(), wxBRUSHSTYLE_SOLID));
	}

	/* Outline */
	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint    (time_x (position) + 1,	      y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len) - 1,     y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len) - 1,     y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position) + 1,	      y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position) + 1,	      y_pos (_track.get()) + 4);
	gc->StrokePath (path);
	gc->FillPath (path);

	/* Reel split points */
	gc->SetPen (*wxThePenList->FindOrCreatePen (foreground_colour(), 1, wxPENSTYLE_DOT));
	BOOST_FOREACH (DCPTime i, cont->reel_split_points ()) {
		path = gc->CreatePath ();
		path.MoveToPoint (time_x (i), y_pos (_track.get()) + 4);
		path.AddLineToPoint (time_x (i), y_pos (_track.get() + 1) - 4);
		gc->StrokePath (path);
	}

	/* Overlaps */
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (foreground_colour(), wxBRUSHSTYLE_CROSSDIAG_HATCH));
	for (list<dcpomatic::Rect<int> >::const_iterator i = overlaps.begin(); i != overlaps.end(); ++i) {
		gc->DrawRectangle (i->x, i->y + 4, i->width, i->height - 8);
	}

	/* Label text */
	wxString name = std_to_wx (cont->summary());
	wxDouble name_width;
	wxDouble name_height;
	wxDouble name_descent;
	wxDouble name_leading;
	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT, foreground_colour ()));
	gc->GetTextExtent (name, &name_width, &name_height, &name_descent, &name_leading);
	gc->Clip (wxRegion (time_x (position), y_pos (_track.get()), len.seconds() * _timeline.pixels_per_second().get_value_or(0), _timeline.track_height()));
	gc->DrawText (name, time_x (position) + 12, y_pos (_track.get() + 1) - name_height - 4);
	gc->ResetClip ();
}

int
TimelineContentView::y_pos (int t) const
{
	return _timeline.tracks_position().y + t * _timeline.track_height();
}

void
TimelineContentView::content_changed (int p)
{
	ensure_ui_thread ();

	if (p == ContentProperty::POSITION || p == ContentProperty::LENGTH) {
		force_redraw ();
	}
}

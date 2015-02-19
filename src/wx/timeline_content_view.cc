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

#include "timeline_content_view.h"
#include "timeline.h"
#include "wx_util.h"
#include "lib/content.h"
#include <wx/graphics.h>

using boost::shared_ptr;

TimelineContentView::TimelineContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineView (tl)
	, _content (c)
	, _selected (false)
{
	_content_connection = c->Changed.connect (bind (&TimelineContentView::content_changed, this, _2, _3));
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
		time_x (content->position ()) - 8,
		y_pos (_track.get()) - 8,
		content->length_after_trim().seconds() * _timeline.pixels_per_second().get_value_or(0) + 16,
		_timeline.track_height() + 16
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
TimelineContentView::do_paint (wxGraphicsContext* gc)
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

	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint    (time_x (position),	      y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len), y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len), y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position),	      y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position),	      y_pos (_track.get()) + 4);
	gc->StrokePath (path);
	gc->FillPath (path);
	
	wxString name = wxString::Format (wxT ("%s [%s]"), std_to_wx (cont->summary()).data(), type().data());
	wxDouble name_width;
	wxDouble name_height;
	wxDouble name_descent;
	wxDouble name_leading;
	gc->GetTextExtent (name, &name_width, &name_height, &name_descent, &name_leading);
	
	gc->Clip (wxRegion (time_x (position), y_pos (_track.get()), len.seconds() * _timeline.pixels_per_second().get_value_or(0), _timeline.track_height()));
	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT, foreground_colour ()));
	gc->DrawText (name, time_x (position) + 12, y_pos (_track.get() + 1) - name_height - 4);
	gc->ResetClip ();
}

int
TimelineContentView::y_pos (int t) const
{
	return _timeline.tracks_position().y + t * _timeline.track_height();
}

void
TimelineContentView::content_changed (int p, bool frequent)
{
	ensure_ui_thread ();
	
	if (p == ContentProperty::POSITION || p == ContentProperty::LENGTH) {
		force_redraw ();
	}
	
	if (!frequent) {
		_timeline.setup_pixels_per_second ();
		_timeline.Refresh ();
	}
}


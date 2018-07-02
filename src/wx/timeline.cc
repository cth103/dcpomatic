/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "film_editor.h"
#include "timeline.h"
#include "timeline_time_axis_view.h"
#include "timeline_reels_view.h"
#include "timeline_labels_view.h"
#include "timeline_video_content_view.h"
#include "timeline_audio_content_view.h"
#include "timeline_subtitle_content_view.h"
#include "timeline_atmos_content_view.h"
#include "content_panel.h"
#include "wx_util.h"
#include "lib/film.h"
#include "lib/playlist.h"
#include "lib/image_content.h"
#include "lib/timer.h"
#include "lib/audio_content.h"
#include "lib/subtitle_content.h"
#include "lib/video_content.h"
#include "lib/atmos_mxf_content.h"
#include <wx/graphics.h>
#include <boost/weak_ptr.hpp>
#include <boost/foreach.hpp>
#include <list>
#include <iostream>

using std::list;
using std::cout;
using std::min;
using std::max;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using boost::optional;

Timeline::Timeline (wxWindow* parent, ContentPanel* cp, shared_ptr<Film> film)
	: wxPanel (parent, wxID_ANY)
	, _labels_panel (new wxPanel (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE))
	, _main_canvas (new wxScrolledCanvas (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE))
	, _content_panel (cp)
	, _film (film)
	, _time_axis_view (new TimelineTimeAxisView (*this, 64))
	, _reels_view (new TimelineReelsView (*this, 32))
	, _labels_view (new TimelineLabelsView (*this))
	, _tracks (0)
	, _left_down (false)
	, _down_view_position (0)
	, _first_move (false)
	, _menu (this)
	, _snap (true)
	, _tool (SELECT)
	, _x_scroll_rate (16)
	, _y_scroll_rate (16)
{
#ifndef __WXOSX__
	_labels_panel->SetDoubleBuffered (true);
	_main_canvas->SetDoubleBuffered (true);
#endif

	wxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	sizer->Add (_labels_panel, 0, wxEXPAND);
	sizer->Add (_main_canvas, 1, wxEXPAND);
	SetSizer (sizer);

	_labels_panel->Bind (wxEVT_PAINT,      boost::bind (&Timeline::paint_labels, this));
	_main_canvas->Bind  (wxEVT_PAINT,      boost::bind (&Timeline::paint_main,   this));
	_main_canvas->Bind  (wxEVT_LEFT_DOWN,  boost::bind (&Timeline::left_down,    this, _1));
	_main_canvas->Bind  (wxEVT_LEFT_UP,    boost::bind (&Timeline::left_up,      this, _1));
	_main_canvas->Bind  (wxEVT_RIGHT_DOWN, boost::bind (&Timeline::right_down,   this, _1));
	_main_canvas->Bind  (wxEVT_MOTION,     boost::bind (&Timeline::mouse_moved,  this, _1));
	_main_canvas->Bind  (wxEVT_SIZE,       boost::bind (&Timeline::resized,      this));

	film_changed (Film::CONTENT);

	SetMinSize (wxSize (640, 4 * track_height() + 96));

	_film_changed_connection = film->Changed.connect (bind (&Timeline::film_changed, this, _1));
	_film_content_changed_connection = film->ContentChanged.connect (bind (&Timeline::film_content_changed, this, _2, _3));

	_pixels_per_second = max (0.01, static_cast<double>(640) / film->length().seconds ());

	setup_scrollbars ();
	_main_canvas->EnableScrolling (true, true);
}

void
Timeline::paint_labels ()
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	_labels_view->paint (gc, list<dcpomatic::Rect<int> >());

	delete gc;
}

void
Timeline::paint_main ()
{
	wxPaintDC dc (this);
	_main_canvas->DoPrepareDC (dc);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int vsx, vsy;
	_main_canvas->GetViewStart (&vsx, &vsy);
	gc->Translate (-vsx * _x_scroll_rate, -vsy * _y_scroll_rate);

	gc->SetAntialiasMode (wxANTIALIAS_DEFAULT);

	BOOST_FOREACH (shared_ptr<TimelineView> i, _views) {

		shared_ptr<TimelineContentView> ic = dynamic_pointer_cast<TimelineContentView> (i);

		/* Find areas of overlap with other content views, so that we can plot them */
		list<dcpomatic::Rect<int> > overlaps;
		BOOST_FOREACH (shared_ptr<TimelineView> j, _views) {
			shared_ptr<TimelineContentView> jc = dynamic_pointer_cast<TimelineContentView> (j);
			/* No overlap with non-content views, views no different tracks, audio views or non-active views */
			if (!ic || !jc || i == j || ic->track() != jc->track() || ic->track().get_value_or(2) >= 2 || !ic->active() || !jc->active()) {
				continue;
			}

			optional<dcpomatic::Rect<int> > r = j->bbox().intersection (i->bbox());
			if (r) {
				overlaps.push_back (r.get ());
			}
		}

		i->paint (gc, overlaps);
	}

	if (_zoom_point) {
		/* Translate back as _down_point and _zoom_point do not take scroll into account */
		gc->Translate (vsx * _x_scroll_rate, vsy * _y_scroll_rate);
		gc->SetPen (*wxBLACK_PEN);
		gc->SetBrush (*wxTRANSPARENT_BRUSH);
		gc->DrawRectangle (
			min (_down_point.x, _zoom_point->x),
			min (_down_point.y, _zoom_point->y),
			fabs (_down_point.x - _zoom_point->x),
			fabs (_down_point.y - _zoom_point->y)
			);
	}

	delete gc;
}

void
Timeline::film_changed (Film::Property p)
{
	if (p == Film::CONTENT || p == Film::REEL_TYPE || p == Film::REEL_LENGTH) {
		ensure_ui_thread ();
		recreate_views ();
	} else if (p == Film::CONTENT_ORDER) {
		Refresh ();
	}
}

void
Timeline::recreate_views ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_views.clear ();
	_views.push_back (_time_axis_view);
	_views.push_back (_reels_view);

	BOOST_FOREACH (shared_ptr<Content> i, film->content ()) {
		if (i->video) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineVideoContentView (*this, i)));
		}

		if (i->audio && !i->audio->mapping().mapped_output_channels().empty ()) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineAudioContentView (*this, i)));
		}

		if (i->subtitle) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineSubtitleContentView (*this, i)));
		}

		if (dynamic_pointer_cast<AtmosMXFContent> (i)) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineAtmosContentView (*this, i)));
		}
	}

	assign_tracks ();
	setup_scrollbars ();
	Refresh ();
}

void
Timeline::film_content_changed (int property, bool frequent)
{
	ensure_ui_thread ();

	if (property == AudioContentProperty::STREAMS) {
		recreate_views ();
	} else if (!frequent) {
		setup_scrollbars ();
		Refresh ();
	}
}

template <class T>
int
place (TimelineViewList& views, int& tracks)
{
	int const base = tracks;

	BOOST_FOREACH (shared_ptr<TimelineView> i, views) {
		if (!dynamic_pointer_cast<T>(i)) {
			continue;
		}

		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (i);

		int t = base;

		shared_ptr<Content> content = cv->content();
		DCPTimePeriod const content_period (content->position(), content->end());

		while (true) {
			TimelineViewList::iterator j = views.begin();
			while (j != views.end()) {
				shared_ptr<T> test = dynamic_pointer_cast<T> (*j);
				if (!test) {
					++j;
					continue;
				}

				shared_ptr<Content> test_content = test->content();
				if (
					test->track() && test->track().get() == t &&
					content_period.overlap(DCPTimePeriod(test_content->position(), test_content->end()))) {
					/* we have an overlap on track `t' */
					++t;
					break;
				}

				++j;
			}

			if (j == views.end ()) {
				/* no overlap on `t' */
				break;
			}
		}

		cv->set_track (t);
		tracks = max (tracks, t + 1);
	}

	return tracks - base;
}

void
Timeline::assign_tracks ()
{
	/* Tracks are:
	   Video (mono or left-eye)
	   Video (right-eye)
	   Subtitle 1
	   Subtitle 2
	   Subtitle N
	   Atmos
	   Audio 1
	   Audio 2
	   Audio N
	*/

	_tracks = 0;

	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> c = dynamic_pointer_cast<TimelineContentView> (*i);
		if (c) {
			c->unset_track ();
		}
	}

	/* Video */

	bool have_3d = false;
	BOOST_FOREACH (shared_ptr<TimelineView> i, _views) {
		shared_ptr<TimelineVideoContentView> cv = dynamic_pointer_cast<TimelineVideoContentView> (i);
		if (!cv) {
			continue;
		}

		/* Video on tracks 0 and maybe 1 (left and right eye) */
		if (cv->content()->video->frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) {
			cv->set_track (1);
			_tracks = max (_tracks, 2);
			have_3d = true;
		} else {
			cv->set_track (0);
		}
	}

	_tracks = max (_tracks, 1);

	/* Subtitle */

	int const subtitle_tracks = place<TimelineSubtitleContentView> (_views, _tracks);

	/* Atmos */

	bool have_atmos = false;
	BOOST_FOREACH (shared_ptr<TimelineView> i, _views) {
		shared_ptr<TimelineVideoContentView> cv = dynamic_pointer_cast<TimelineVideoContentView> (i);
		if (!cv) {
			continue;
		}
		if (dynamic_pointer_cast<TimelineAtmosContentView> (i)) {
			cv->set_track (_tracks - 1);
			have_atmos = true;
		}
	}

	if (have_atmos) {
		++_tracks;
	}

	/* Audio */

	place<TimelineAudioContentView> (_views, _tracks);

	_labels_view->set_3d (have_3d);
	_labels_view->set_subtitle_tracks (subtitle_tracks);
	_labels_view->set_atmos (have_atmos);

	_time_axis_view->set_y (tracks() * track_height() + 64);
	_reels_view->set_y (8);
}

int
Timeline::tracks () const
{
	return _tracks;
}

void
Timeline::setup_scrollbars ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film || !_pixels_per_second) {
		return;
	}
	_main_canvas->SetVirtualSize (*_pixels_per_second * film->length().seconds(), tracks() * track_height() + 96);
	_main_canvas->SetScrollRate (_x_scroll_rate, _y_scroll_rate);
}

shared_ptr<TimelineView>
Timeline::event_to_view (wxMouseEvent& ev)
{
	/* Search backwards through views so that we find the uppermost one first */
	TimelineViewList::reverse_iterator i = _views.rbegin();
	Position<int> const p (ev.GetX(), ev.GetY());
	while (i != _views.rend() && !(*i)->bbox().contains (p)) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		++i;
	}

	if (i == _views.rend ()) {
		return shared_ptr<TimelineView> ();
	}

	return *i;
}

void
Timeline::left_down (wxMouseEvent& ev)
{
	_left_down = true;
	_down_point = ev.GetPosition ();

	switch (_tool) {
	case SELECT:
		left_down_select (ev);
		break;
	case ZOOM:
		/* Nothing to do */
		break;
	}
}

void
Timeline::left_down_select (wxMouseEvent& ev)
{
	shared_ptr<TimelineView> view = event_to_view (ev);
	shared_ptr<TimelineContentView> content_view = dynamic_pointer_cast<TimelineContentView> (view);

	_down_view.reset ();

	if (content_view) {
		_down_view = content_view;
		_down_view_position = content_view->content()->position ();
	}

	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		if (!cv) {
			continue;
		}

		if (!ev.ShiftDown ()) {
			cv->set_selected (view == *i);
		}
	}

	if (content_view && ev.ShiftDown ()) {
		content_view->set_selected (!content_view->selected ());
	}

	_first_move = false;

	if (_down_view) {
		/* Pre-compute the points that we might snap to */
		for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
			shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
			if (!cv || cv == _down_view || cv->content() == _down_view->content()) {
				continue;
			}

			_start_snaps.push_back (cv->content()->position());
			_end_snaps.push_back (cv->content()->position());
			_start_snaps.push_back (cv->content()->end());
			_end_snaps.push_back (cv->content()->end());

			BOOST_FOREACH (DCPTime i, cv->content()->reel_split_points()) {
				_start_snaps.push_back (i);
			}
		}

		/* Tell everyone that things might change frequently during the drag */
		_down_view->content()->set_change_signals_frequent (true);
	}
}

void
Timeline::left_up (wxMouseEvent& ev)
{
	_left_down = false;

	switch (_tool) {
	case SELECT:
		left_up_select (ev);
		break;
	case ZOOM:
		left_up_zoom (ev);
		break;
	}
}

void
Timeline::left_up_select (wxMouseEvent& ev)
{
	if (_down_view) {
		_down_view->content()->set_change_signals_frequent (false);
	}

	_content_panel->set_selection (selected_content ());
	set_position_from_event (ev);

	/* Clear up up the stuff we don't do during drag */
	assign_tracks ();
	setup_scrollbars ();
	Refresh ();

	_start_snaps.clear ();
	_end_snaps.clear ();
}

void
Timeline::left_up_zoom (wxMouseEvent& ev)
{
	_zoom_point = ev.GetPosition ();

	int vsx, vsy;
	_main_canvas->GetViewStart (&vsx, &vsy);

	wxPoint top_left(min(_down_point.x, _zoom_point->x), min(_down_point.y, _zoom_point->y));
	wxPoint bottom_right(max(_down_point.x, _zoom_point->x), max(_down_point.y, _zoom_point->y));

	DCPTime time_left = DCPTime::from_seconds((top_left.x + vsx) / *_pixels_per_second);
	DCPTime time_right = DCPTime::from_seconds((bottom_right.x + vsx) / *_pixels_per_second);
	_pixels_per_second = GetSize().GetWidth() / (time_right.seconds() - time_left.seconds());
	setup_scrollbars ();
	_main_canvas->Scroll (time_left.seconds() * *_pixels_per_second / _x_scroll_rate, wxDefaultCoord);

	_zoom_point = optional<wxPoint> ();
	Refresh ();
}

void
Timeline::mouse_moved (wxMouseEvent& ev)
{
	switch (_tool) {
	case SELECT:
		mouse_moved_select (ev);
		break;
	case ZOOM:
		mouse_moved_zoom (ev);
		break;
	}
}

void
Timeline::mouse_moved_select (wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	set_position_from_event (ev);
}

void
Timeline::mouse_moved_zoom (wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	_zoom_point = ev.GetPosition ();
	Refresh ();
}

void
Timeline::right_down (wxMouseEvent& ev)
{
	switch (_tool) {
	case SELECT:
		right_down_select (ev);
		break;
	case ZOOM:
		/* Zoom out */
		_pixels_per_second = *_pixels_per_second / 2;
		setup_scrollbars ();
		break;
	}
}

void
Timeline::right_down_select (wxMouseEvent& ev)
{
	shared_ptr<TimelineView> view = event_to_view (ev);
	shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (view);
	if (!cv) {
		return;
	}

	if (!cv->selected ()) {
		clear_selection ();
		cv->set_selected (true);
	}

	_menu.popup (_film, selected_content (), selected_views (), ev.GetPosition ());
}

void
Timeline::maybe_snap (DCPTime a, DCPTime b, optional<DCPTime>& nearest_distance) const
{
	DCPTime const d = a - b;
	if (!nearest_distance || d.abs() < nearest_distance.get().abs()) {
		nearest_distance = d;
	}
}

void
Timeline::set_position_from_event (wxMouseEvent& ev)
{
	if (!_pixels_per_second) {
		return;
	}

	double const pps = _pixels_per_second.get ();

	wxPoint const p = ev.GetPosition();

	if (!_first_move) {
		/* We haven't moved yet; in that case, we must move the mouse some reasonable distance
		   before the drag is considered to have started.
		*/
		int const dist = sqrt (pow (p.x - _down_point.x, 2) + pow (p.y - _down_point.y, 2));
		if (dist < 8) {
			return;
		}
		_first_move = true;
	}

	if (!_down_view) {
		return;
	}

	DCPTime new_position = _down_view_position + DCPTime::from_seconds ((p.x - _down_point.x) / pps);

	if (_snap) {

		DCPTime const new_end = new_position + _down_view->content()->length_after_trim();
		/* Signed `distance' to nearest thing (i.e. negative is left on the timeline,
		   positive is right).
		*/
		optional<DCPTime> nearest_distance;

		/* Find the nearest snap point */

		BOOST_FOREACH (DCPTime i, _start_snaps) {
			maybe_snap (i, new_position, nearest_distance);
		}

		BOOST_FOREACH (DCPTime i, _end_snaps) {
			maybe_snap (i, new_end, nearest_distance);
		}

		if (nearest_distance) {
			/* Snap if it's close; `close' means within a proportion of the time on the timeline */
			if (nearest_distance.get().abs() < DCPTime::from_seconds ((width() / pps) / 64)) {
				new_position += nearest_distance.get ();
			}
		}
	}

	if (new_position < DCPTime ()) {
		new_position = DCPTime ();
	}

	_down_view->content()->set_position (new_position);

	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	film->set_sequence (false);
}

void
Timeline::force_redraw (dcpomatic::Rect<int> const & r)
{
	RefreshRect (wxRect (r.x, r.y, r.width, r.height), false);
}

shared_ptr<const Film>
Timeline::film () const
{
	return _film.lock ();
}

void
Timeline::resized ()
{
	setup_scrollbars ();
}

void
Timeline::clear_selection ()
{
	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		if (cv) {
			cv->set_selected (false);
		}
	}
}

TimelineContentViewList
Timeline::selected_views () const
{
	TimelineContentViewList sel;

	for (TimelineViewList::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		if (cv && cv->selected()) {
			sel.push_back (cv);
		}
	}

	return sel;
}

ContentList
Timeline::selected_content () const
{
	ContentList sel;
	TimelineContentViewList views = selected_views ();

	for (TimelineContentViewList::const_iterator i = views.begin(); i != views.end(); ++i) {
		sel.push_back ((*i)->content ());
	}

	return sel;
}

void
Timeline::set_selection (ContentList selection)
{
	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		if (cv) {
			cv->set_selected (find (selection.begin(), selection.end(), cv->content ()) != selection.end ());
		}
	}
}

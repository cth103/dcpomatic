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

#include <list>
#include <wx/graphics.h>
#include <boost/weak_ptr.hpp>
#include "lib/film.h"
#include "lib/playlist.h"
#include "lib/image_content.h"
#include "film_editor.h"
#include "timeline.h"
#include "timeline_time_axis_view.h"
#include "timeline_video_content_view.h"
#include "timeline_audio_content_view.h"
#include "timeline_subtitle_content_view.h"
#include "content_panel.h"
#include "wx_util.h"

using std::list;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using boost::optional;

Timeline::Timeline (wxWindow* parent, ContentPanel* cp, shared_ptr<Film> film)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _content_panel (cp)
	, _film (film)
	, _time_axis_view (new TimelineTimeAxisView (*this, 32))
	, _tracks (0)
	, _left_down (false)
	, _down_view_position (0)
	, _first_move (false)
	, _menu (this)
	, _snap (true)
{
#ifndef __WXOSX__
	SetDoubleBuffered (true);
#endif	

	Bind (wxEVT_PAINT,      boost::bind (&Timeline::paint,       this));
	Bind (wxEVT_LEFT_DOWN,  boost::bind (&Timeline::left_down,   this, _1));
	Bind (wxEVT_LEFT_UP,    boost::bind (&Timeline::left_up,     this, _1));
	Bind (wxEVT_RIGHT_DOWN, boost::bind (&Timeline::right_down,  this, _1));
	Bind (wxEVT_MOTION,     boost::bind (&Timeline::mouse_moved, this, _1));
	Bind (wxEVT_SIZE,       boost::bind (&Timeline::resized,     this));

	playlist_changed ();

	SetMinSize (wxSize (640, tracks() * track_height() + 96));

	_playlist_changed_connection = film->playlist()->Changed.connect (bind (&Timeline::playlist_changed, this));
	_playlist_content_changed_connection = film->playlist()->ContentChanged.connect (bind (&Timeline::playlist_content_changed, this, _2));
}

void
Timeline::paint ()
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		(*i)->paint (gc);
	}

	delete gc;
}

void
Timeline::playlist_changed ()
{
	ensure_ui_thread ();
	recreate_views ();
}

void
Timeline::recreate_views ()
{
	shared_ptr<const Film> fl = _film.lock ();
	if (!fl) {
		return;
	}

	_views.clear ();
	_views.push_back (_time_axis_view);

	ContentList content = fl->playlist()->content ();

	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		if (dynamic_pointer_cast<VideoContent> (*i)) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineVideoContentView (*this, *i)));
		}

		shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (*i);
		if (ac && !ac->audio_mapping().mapped_dcp_channels().empty ()) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineAudioContentView (*this, *i)));
		}

		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (*i);
		if (sc && sc->has_subtitles ()) {
			_views.push_back (shared_ptr<TimelineView> (new TimelineSubtitleContentView (*this, sc)));
		}
	}

	assign_tracks ();
	setup_pixels_per_second ();
	Refresh ();
}

void
Timeline::playlist_content_changed (int property)
{
	ensure_ui_thread ();

	if (property == ContentProperty::POSITION) {
		assign_tracks ();
		if (!_left_down) {
			/* Only do this if we are not dragging, as it's confusing otherwise */
			setup_pixels_per_second ();
		}
		Refresh ();
	} else if (property == AudioContentProperty::AUDIO_MAPPING) {
		recreate_views ();
	}
}

void
Timeline::assign_tracks ()
{
	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> c = dynamic_pointer_cast<TimelineContentView> (*i);
		if (c) {
			c->unset_track ();
		}
	}

	for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
		if (!cv) {
			continue;
		}

		shared_ptr<Content> content = cv->content();

		int t = 0;
		while (true) {
			TimelineViewList::iterator j = _views.begin();
			while (j != _views.end()) {
				shared_ptr<TimelineContentView> test = dynamic_pointer_cast<TimelineContentView> (*j);
				if (!test) {
					++j;
					continue;
				}
				
				shared_ptr<Content> test_content = test->content();
					
				if (test && test->track() && test->track().get() == t) {
					bool const no_overlap =
						(content->position() < test_content->position() && content->end() < test_content->position()) ||
						(content->position() > test_content->end()      && content->end() > test_content->end());
					
					if (!no_overlap) {
						/* we have an overlap on track `t' */
						++t;
						break;
					}
				}
				
				++j;
			}

			if (j == _views.end ()) {
				/* no overlap on `t' */
				break;
			}
		}

		cv->set_track (t);
		_tracks = max (_tracks, t + 1);
	}

	_time_axis_view->set_y (tracks() * track_height() + 32);
}

int
Timeline::tracks () const
{
	return _tracks;
}

void
Timeline::setup_pixels_per_second ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film || film->length() == DCPTime ()) {
		return;
	}

	_pixels_per_second = static_cast<double>(width() - x_offset() * 2) / film->length().seconds ();
}

shared_ptr<TimelineView>
Timeline::event_to_view (wxMouseEvent& ev)
{
	TimelineViewList::iterator i = _views.begin();
	Position<int> const p (ev.GetX(), ev.GetY());
	while (i != _views.end() && !(*i)->bbox().contains (p)) {
		++i;
	}

	if (i == _views.end ()) {
		return shared_ptr<TimelineView> ();
	}

	return *i;
}

void
Timeline::left_down (wxMouseEvent& ev)
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
		
		if (view == *i) {
			_content_panel->set_selection (cv->content ());
		}
	}

	if (content_view && ev.ShiftDown ()) {
		content_view->set_selected (!content_view->selected ());
	}

	_left_down = true;
	_down_point = ev.GetPosition ();
	_first_move = false;

	if (_down_view) {
		_down_view->content()->set_change_signals_frequent (true);
	}
}

void
Timeline::left_up (wxMouseEvent& ev)
{
	_left_down = false;

	if (_down_view) {
		_down_view->content()->set_change_signals_frequent (false);
	}

	set_position_from_event (ev);

	/* We don't do this during drag, and set_position_from_event above
	   might not have changed the position, so do it now.
	*/
	setup_pixels_per_second ();
	Refresh ();
}

void
Timeline::mouse_moved (wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	set_position_from_event (ev);
}

void
Timeline::right_down (wxMouseEvent& ev)
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

		DCPTime const new_end = new_position + _down_view->content()->length_after_trim () - DCPTime (1);
		/* Signed `distance' to nearest thing (i.e. negative is left on the timeline,
		   positive is right).
		*/
		optional<DCPTime> nearest_distance;
		
		/* Find the nearest content edge; this is inefficient */
		for (TimelineViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
			shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView> (*i);
			if (!cv || cv == _down_view || cv->content() == _down_view->content()) {
				continue;
			}

			maybe_snap (cv->content()->position(), new_position, nearest_distance);
			maybe_snap (cv->content()->position(), new_end + DCPTime (1), nearest_distance);
			maybe_snap (cv->content()->end() + DCPTime (1), new_position, nearest_distance);
			maybe_snap (cv->content()->end() + DCPTime (1), new_end, nearest_distance);
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
	film->set_sequence_video (false);
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
	setup_pixels_per_second ();
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

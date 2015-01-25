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

/** @class View
 *  @brief Parent class for components of the timeline (e.g. a piece of content or an axis).
 */
class View : public boost::noncopyable
{
public:
	View (Timeline& t)
		: _timeline (t)
	{

	}

	virtual ~View () {}
		
	void paint (wxGraphicsContext* g)
	{
		_last_paint_bbox = bbox ();
		do_paint (g);
	}
	
	void force_redraw ()
	{
		_timeline.force_redraw (_last_paint_bbox);
		_timeline.force_redraw (bbox ());
	}

	virtual dcpomatic::Rect<int> bbox () const = 0;

protected:
	virtual void do_paint (wxGraphicsContext *) = 0;
	
	int time_x (DCPTime t) const
	{
		return _timeline.tracks_position().x + t.seconds() * _timeline.pixels_per_second().get_value_or (0);
	}
	
	Timeline& _timeline;

private:
	dcpomatic::Rect<int> _last_paint_bbox;
};


/** @class ContentView
 *  @brief Parent class for views of pieces of content.
 */
class ContentView : public View
{
public:
	ContentView (Timeline& tl, shared_ptr<Content> c)
		: View (tl)
		, _content (c)
		, _selected (false)
	{
		_content_connection = c->Changed.connect (bind (&ContentView::content_changed, this, _2, _3));
	}

	dcpomatic::Rect<int> bbox () const
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

	void set_selected (bool s) {
		_selected = s;
		force_redraw ();
	}
	
	bool selected () const {
		return _selected;
	}

	shared_ptr<Content> content () const {
		return _content.lock ();
	}

	void set_track (int t) {
		_track = t;
	}

	void unset_track () {
		_track = boost::optional<int> ();
	}

	optional<int> track () const {
		return _track;
	}

	virtual wxString type () const = 0;
	virtual wxColour background_colour () const = 0;
	virtual wxColour foreground_colour () const = 0;
	
private:

	void do_paint (wxGraphicsContext* gc)
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

	int y_pos (int t) const
	{
		return _timeline.tracks_position().y + t * _timeline.track_height();
	}

	void content_changed (int p, bool frequent)
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

	boost::weak_ptr<Content> _content;
	optional<int> _track;
	bool _selected;

	boost::signals2::scoped_connection _content_connection;
};

/** @class AudioContentView
 *  @brief Timeline view for AudioContent.
 */
class AudioContentView : public ContentView
{
public:
	AudioContentView (Timeline& tl, shared_ptr<Content> c)
		: ContentView (tl, c)
	{}
	
private:
	wxString type () const
	{
		return _("audio");
	}

	wxColour background_colour () const
	{
		return wxColour (149, 121, 232, 255);
	}

	wxColour foreground_colour () const
	{
		return wxColour (0, 0, 0, 255);
	}
};

/** @class AudioContentView
 *  @brief Timeline view for VideoContent.
 */
class VideoContentView : public ContentView
{
public:
	VideoContentView (Timeline& tl, shared_ptr<Content> c)
		: ContentView (tl, c)
	{}

private:	

	wxString type () const
	{
		if (dynamic_pointer_cast<ImageContent> (content ()) && content()->number_of_paths() == 1) {
			return _("still");
		} else {
			return _("video");
		}
	}

	wxColour background_colour () const
	{
		return wxColour (242, 92, 120, 255);
	}

	wxColour foreground_colour () const
	{
		return wxColour (0, 0, 0, 255);
	}
};

/** @class AudioContentView
 *  @brief Timeline view for SubtitleContent.
 */
class SubtitleContentView : public ContentView
{
public:
	SubtitleContentView (Timeline& tl, shared_ptr<SubtitleContent> c)
		: ContentView (tl, c)
		, _subtitle_content (c)
	{}

private:
	wxString type () const
	{
		return _("subtitles");
	}

	wxColour background_colour () const
	{
		shared_ptr<SubtitleContent> sc = _subtitle_content.lock ();
		if (!sc || !sc->use_subtitles ()) {
			return wxColour (210, 210, 210, 128);
		}
		
		return wxColour (163, 255, 154, 255);
	}

	wxColour foreground_colour () const
	{
		shared_ptr<SubtitleContent> sc = _subtitle_content.lock ();
		if (!sc || !sc->use_subtitles ()) {
			return wxColour (180, 180, 180, 128);
		}

		return wxColour (0, 0, 0, 255);
	}

	boost::weak_ptr<SubtitleContent> _subtitle_content;
};

class TimeAxisView : public View
{
public:
	TimeAxisView (Timeline& tl, int y)
		: View (tl)
		, _y (y)
	{}
	
	dcpomatic::Rect<int> bbox () const
	{
		return dcpomatic::Rect<int> (0, _y - 4, _timeline.width(), 24);
	}

	void set_y (int y)
	{
		_y = y;
		force_redraw ();
	}

private:	

	void do_paint (wxGraphicsContext* gc)
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

private:
	int _y;
};


Timeline::Timeline (wxWindow* parent, ContentPanel* cp, shared_ptr<Film> film)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _content_panel (cp)
	, _film (film)
	, _time_axis_view (new TimeAxisView (*this, 32))
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

	for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		(*i)->paint (gc);
	}

	delete gc;
}

void
Timeline::playlist_changed ()
{
	ensure_ui_thread ();
	
	shared_ptr<const Film> fl = _film.lock ();
	if (!fl) {
		return;
	}

	_views.clear ();
	_views.push_back (_time_axis_view);

	ContentList content = fl->playlist()->content ();

	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		if (dynamic_pointer_cast<VideoContent> (*i)) {
			_views.push_back (shared_ptr<View> (new VideoContentView (*this, *i)));
		}
		if (dynamic_pointer_cast<AudioContent> (*i)) {
			_views.push_back (shared_ptr<View> (new AudioContentView (*this, *i)));
		}

		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (*i);
		if (sc && sc->has_subtitles ()) {
			_views.push_back (shared_ptr<View> (new SubtitleContentView (*this, sc)));
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
		setup_pixels_per_second ();
		Refresh ();
	}
}

void
Timeline::assign_tracks ()
{
	for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> c = dynamic_pointer_cast<ContentView> (*i);
		if (c) {
			c->unset_track ();
		}
	}

	for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
		if (!cv) {
			continue;
		}

		shared_ptr<Content> content = cv->content();

		int t = 0;
		while (true) {
			ViewList::iterator j = _views.begin();
			while (j != _views.end()) {
				shared_ptr<ContentView> test = dynamic_pointer_cast<ContentView> (*j);
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

shared_ptr<View>
Timeline::event_to_view (wxMouseEvent& ev)
{
	ViewList::iterator i = _views.begin();
	Position<int> const p (ev.GetX(), ev.GetY());
	while (i != _views.end() && !(*i)->bbox().contains (p)) {
		++i;
	}

	if (i == _views.end ()) {
		return shared_ptr<View> ();
	}

	return *i;
}

void
Timeline::left_down (wxMouseEvent& ev)
{
	shared_ptr<View> view = event_to_view (ev);
	shared_ptr<ContentView> content_view = dynamic_pointer_cast<ContentView> (view);

	_down_view.reset ();

	if (content_view) {
		_down_view = content_view;
		_down_view_position = content_view->content()->position ();
	}

	for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
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
	shared_ptr<View> view = event_to_view (ev);
	shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (view);
	if (!cv) {
		return;
	}

	if (!cv->selected ()) {
		clear_selection ();
		cv->set_selected (true);
	}

	_menu.popup (_film, selected_content (), ev.GetPosition ());
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

		DCPTime const new_end = new_position + _down_view->content()->length_after_trim ();
		/* Signed `distance' to nearest thing (i.e. negative is left on the timeline,
		   positive is right).
		*/
		optional<DCPTime> nearest_distance;
		
		/* Find the nearest content edge; this is inefficient */
		for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
			shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
			if (!cv || cv == _down_view) {
				continue;
			}

			maybe_snap (cv->content()->position(), new_position, nearest_distance);
			maybe_snap (cv->content()->position(), new_end, nearest_distance);
			maybe_snap (cv->content()->end(), new_position, nearest_distance);
			maybe_snap (cv->content()->end(), new_end, nearest_distance);
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
	for (ViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
		if (cv) {
			cv->set_selected (false);
		}
	}
}

Timeline::ContentViewList
Timeline::selected_views () const
{
	ContentViewList sel;
	
	for (ViewList::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
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
	ContentViewList views = selected_views ();
	
	for (ContentViewList::const_iterator i = views.begin(); i != views.end(); ++i) {
		sel.push_back ((*i)->content ());
	}

	return sel;
}

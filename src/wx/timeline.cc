/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "film_editor.h"
#include "timeline.h"
#include "wx_util.h"

using std::list;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;

class View : public boost::noncopyable
{
public:
	View (Timeline& t)
		: _timeline (t)
	{

	}
		
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
	
	int time_x (Time t) const
	{
		return _timeline.tracks_position().x + t * _timeline.pixels_per_time_unit();
	}
	
	Timeline& _timeline;

private:
	dcpomatic::Rect<int> _last_paint_bbox;
};

class ContentView : public View
{
public:
	ContentView (Timeline& tl, shared_ptr<Content> c, int t)
		: View (tl)
		, _content (c)
		, _track (t)
		, _selected (false)
	{
		_content_connection = c->Changed.connect (bind (&ContentView::content_changed, this, _2));
	}

	dcpomatic::Rect<int> bbox () const
	{
		shared_ptr<const Film> film = _timeline.film ();
		if (!film) {
			return dcpomatic::Rect<int> ();
		}
		
		return dcpomatic::Rect<int> (
			time_x (_content->start ()) - 8,
			y_pos (_track) - 8,
			_content->length () * _timeline.pixels_per_time_unit() + 16,
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
		return _content;
	}

	void set_track (int t) {
		_track = t;
	}

	int track () const {
		return _track;
	}

	virtual wxString type () const = 0;
	virtual wxColour colour () const = 0;
	
private:

	void do_paint (wxGraphicsContext* gc)
	{
		shared_ptr<const Film> film = _timeline.film ();
		if (!film) {
			return;
		}

		Time const start = _content->start ();
		Time const len = _content->length ();

		wxColour selected (colour().Red() / 2, colour().Green() / 2, colour().Blue() / 2);

		gc->SetPen (*wxBLACK_PEN);
		
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 4, wxPENSTYLE_SOLID));
		if (_selected) {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (selected, wxBRUSHSTYLE_SOLID));
		} else {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (colour(), wxBRUSHSTYLE_SOLID));
		}

		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint    (time_x (start),	   y_pos (_track) + 4);
		path.AddLineToPoint (time_x (start + len), y_pos (_track) + 4);
		path.AddLineToPoint (time_x (start + len), y_pos (_track + 1) - 4);
		path.AddLineToPoint (time_x (start),	   y_pos (_track + 1) - 4);
		path.AddLineToPoint (time_x (start),	   y_pos (_track) + 4);
		gc->StrokePath (path);
		gc->FillPath (path);

		wxString name = wxString::Format (wxT ("%s [%s]"), std_to_wx (_content->file().filename().string()).data(), type().data());
		wxDouble name_width;
		wxDouble name_height;
		wxDouble name_descent;
		wxDouble name_leading;
		gc->GetTextExtent (name, &name_width, &name_height, &name_descent, &name_leading);
		
		gc->Clip (wxRegion (time_x (start), y_pos (_track), len * _timeline.pixels_per_time_unit(), _timeline.track_height()));
		gc->DrawText (name, time_x (start) + 12, y_pos (_track + 1) - name_height - 4);
		gc->ResetClip ();
	}

	int y_pos (int t) const
	{
		return _timeline.tracks_position().y + t * _timeline.track_height();
	}

	void content_changed (int p)
	{
		if (p == ContentProperty::START || p == ContentProperty::LENGTH) {
			force_redraw ();
		}
	}

	/* This must be a shared_ptr, not a weak_ptr, as in the looped case this
	   will be the only remaining pointer to the looped content that we get
	   from the playlist.
	*/
	boost::shared_ptr<Content> _content;
	int _track;
	bool _selected;

	boost::signals2::scoped_connection _content_connection;
};

class AudioContentView : public ContentView
{
public:
	AudioContentView (Timeline& tl, shared_ptr<Content> c, int t)
		: ContentView (tl, c, t)
	{}
	
private:
	wxString type () const
	{
		return _("audio");
	}

	wxColour colour () const
	{
		return wxColour (149, 121, 232, 255);
	}
};

class VideoContentView : public ContentView
{
public:
	VideoContentView (Timeline& tl, shared_ptr<Content> c, int t)
		: ContentView (tl, c, t)
	{}

private:	

	wxString type () const
	{
		if (dynamic_pointer_cast<FFmpegContent> (content ())) {
			return _("video");
		} else {
			return _("still");
		}
	}

	wxColour colour () const
	{
		return wxColour (242, 92, 120, 255);
	}
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
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));
		
		int mark_interval = rint (128 / (TIME_HZ * _timeline.pixels_per_time_unit ()));
		if (mark_interval > 5) {
			mark_interval -= mark_interval % 5;
		}
		if (mark_interval > 10) {
			mark_interval -= mark_interval % 10;
		}
		if (mark_interval > 60) {
			mark_interval -= mark_interval % 60;
		}
		if (mark_interval > 3600) {
			mark_interval -= mark_interval % 3600;
		}
		
		if (mark_interval < 1) {
			mark_interval = 1;
		}

		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (_timeline.x_offset(), _y);
		path.AddLineToPoint (_timeline.width(), _y);
		gc->StrokePath (path);

		Time t = 0;
		while ((t * _timeline.pixels_per_time_unit()) < _timeline.width()) {
			wxGraphicsPath path = gc->CreatePath ();
			path.MoveToPoint (time_x (t), _y - 4);
			path.AddLineToPoint (time_x (t), _y + 4);
			gc->StrokePath (path);

			int tc = t / TIME_HZ;
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
			
			int const tx = _timeline.x_offset() + t * _timeline.pixels_per_time_unit();
			if ((tx + str_width) < _timeline.width()) {
				gc->DrawText (str, time_x (t), _y + 16);
			}
			
			t += mark_interval * TIME_HZ;
		}
	}

private:
	int _y;
};

Timeline::Timeline (wxWindow* parent, FilmEditor* ed, shared_ptr<Film> film)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _film_editor (ed)
	, _film (film)
	, _time_axis_view (new TimeAxisView (*this, 32))
	, _tracks (0)
	, _pixels_per_time_unit (0)
	, _left_down (false)
	, _down_view_start (0)
	, _first_move (false)
{
#ifndef __WXOSX__
	SetDoubleBuffered (true);
#endif	

	setup_pixels_per_time_unit ();
	
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (Timeline::paint), 0, this);
	Connect (wxID_ANY, wxEVT_LEFT_DOWN, wxMouseEventHandler (Timeline::left_down), 0, this);
	Connect (wxID_ANY, wxEVT_LEFT_UP, wxMouseEventHandler (Timeline::left_up), 0, this);
	Connect (wxID_ANY, wxEVT_MOTION, wxMouseEventHandler (Timeline::mouse_moved), 0, this);
	Connect (wxID_ANY, wxEVT_SIZE, wxSizeEventHandler (Timeline::resized), 0, this);

	playlist_changed ();

	SetMinSize (wxSize (640, tracks() * track_height() + 96));

	_playlist_connection = film->playlist()->Changed.connect (bind (&Timeline::playlist_changed, this));

	_views.push_back (_time_axis_view);
}

void
Timeline::paint (wxPaintEvent &)
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT));

	for (list<shared_ptr<View> >::iterator i = _views.begin(); i != _views.end(); ++i) {
		(*i)->paint (gc);
	}

	delete gc;
}

void
Timeline::playlist_changed ()
{
	shared_ptr<const Film> fl = _film.lock ();
	if (!fl) {
		return;
	}

	_views.clear ();

	Playlist::ContentList content = fl->playlist()->content ();

	for (Playlist::ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		if (dynamic_pointer_cast<VideoContent> (*i)) {
			_views.push_back (shared_ptr<View> (new VideoContentView (*this, *i, 0)));
		}
		if (dynamic_pointer_cast<AudioContent> (*i)) {
			_views.push_back (shared_ptr<View> (new AudioContentView (*this, *i, 0)));
		}
	}

	assign_tracks ();
	Refresh ();
}

void
Timeline::assign_tracks ()
{
	for (list<shared_ptr<View> >::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
		if (cv) {
			cv->set_track (0);
			_tracks = 1;
		}
	}

	for (list<shared_ptr<View> >::iterator i = _views.begin(); i != _views.end(); ++i) {
		shared_ptr<AudioContentView> acv = dynamic_pointer_cast<AudioContentView> (*i);
		if (!acv) {
			continue;
		}
	
		shared_ptr<Content> acv_content = acv->content();
		
		int t = 1;
		while (1) {
			list<shared_ptr<View> >::iterator j = _views.begin();
			while (j != _views.end()) {
				shared_ptr<AudioContentView> test = dynamic_pointer_cast<AudioContentView> (*j);
				if (!test) {
					++j;
					continue;
				}
				
				shared_ptr<Content> test_content = test->content();
					
				if (test && test->track() == t) {
					if ((acv_content->start() < test_content->start() && test_content->start() < acv_content->end()) ||
					    (acv_content->start() < test_content->end()   && test_content->end()   < acv_content->end())) {
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

		acv->set_track (t);
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
Timeline::setup_pixels_per_time_unit ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_pixels_per_time_unit = static_cast<double>(width() - x_offset() * 2) / film->length ();
}

void
Timeline::left_down (wxMouseEvent& ev)
{
	list<shared_ptr<View> >::iterator i = _views.begin();
	Position<int> const p (ev.GetX(), ev.GetY());
	while (i != _views.end() && !(*i)->bbox().contains (p)) {
		++i;
	}

	_down_view.reset ();

	if (i != _views.end ()) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*i);
		if (cv) {
			_down_view = cv;
			_down_view_start = cv->content()->start ();
		}
	}

	for (list<shared_ptr<View> >::iterator j = _views.begin(); j != _views.end(); ++j) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*j);
		if (cv) {
			cv->set_selected (i == j);
			if (i == j) {
				_film_editor->set_selection (cv->content ());
			}
		}
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

	set_start_from_event (ev);
}

void
Timeline::mouse_moved (wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	set_start_from_event (ev);
}

void
Timeline::set_start_from_event (wxMouseEvent& ev)
{
	wxPoint const p = ev.GetPosition();

	if (!_first_move) {
		int const dist = sqrt (pow (p.x - _down_point.x, 2) + pow (p.y - _down_point.y, 2));
		if (dist < 8) {
			return;
		}
		_first_move = true;
	}

	Time const time_diff = (p.x - _down_point.x) / _pixels_per_time_unit;
	if (_down_view) {
		_down_view->content()->set_start (max (static_cast<Time> (0), _down_view_start + time_diff));

		shared_ptr<Film> film = _film.lock ();
		assert (film);
		film->set_sequence_video (false);
	}
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
Timeline::resized (wxSizeEvent &)
{
	setup_pixels_per_time_unit ();
}

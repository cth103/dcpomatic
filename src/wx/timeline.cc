/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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
#include "film.h"
#include "timeline.h"
#include "wx_util.h"
#include "playlist.h"

using std::list;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;

class View
{
public:
	View (Timeline& t)
	        : _timeline (t)
	{

	}
		
	virtual void paint (wxGraphicsContext *) = 0;
	virtual Rect bbox () const = 0;

protected:
	int time_x (Time t) const
	{
		return _timeline.tracks_position().x + t * _timeline.pixels_per_time_unit();
	}
	
	Timeline& _timeline;
};

class ContentView : public View
{
public:
	ContentView (Timeline& tl, shared_ptr<const Content> c, int t)
		: View (tl)
		, _content (c)
		, _track (t)
		, _selected (false)
	{

	}

	void paint (wxGraphicsContext* gc)
	{
		shared_ptr<const Film> film = _timeline.film ();
		shared_ptr<const Content> content = _content.lock ();
		if (!film || !content) {
			return;
		}

		Time const start = content->start ();
		Time const len = content->length ();

		gc->SetPen (*wxBLACK_PEN);
		
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 4, wxPENSTYLE_SOLID));
		if (_selected) {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (200, 200, 200), wxBRUSHSTYLE_SOLID));
		} else {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (colour(), wxBRUSHSTYLE_SOLID));
		}
#else			
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 4, wxSOLID));
		if (_selected) {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (200, 200, 200), wxSOLID));
		} else {
			gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (colour(), wxSOLID));
		}
#endif
		
		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint    (time_x (start),       y_pos (_track));
		path.AddLineToPoint (time_x (start + len), y_pos (_track));
		path.AddLineToPoint (time_x (start + len), y_pos (_track + 1));
		path.AddLineToPoint (time_x (start),       y_pos (_track + 1));
		path.AddLineToPoint (time_x (start),       y_pos (_track));
		gc->StrokePath (path);
		gc->FillPath (path);

		wxString name = wxString::Format (wxT ("%s [%s]"), std_to_wx (content->file().filename().string()).data(), type().data());
		wxDouble name_width;
		wxDouble name_height;
		wxDouble name_descent;
		wxDouble name_leading;
		gc->GetTextExtent (name, &name_width, &name_height, &name_descent, &name_leading);
		
		gc->Clip (wxRegion (time_x (start), y_pos (_track), len * _timeline.pixels_per_time_unit(), _timeline.track_height()));
		gc->DrawText (name, time_x (start) + 12, y_pos (_track + 1) - name_height - 4);
		gc->ResetClip ();
	}

	Rect bbox () const
	{
		shared_ptr<const Film> film = _timeline.film ();
		shared_ptr<const Content> content = _content.lock ();
		if (!film || !content) {
			return Rect ();
		}
		
		return Rect (
			time_x (content->start ()),
			y_pos (_track),
			content->length () * _timeline.pixels_per_time_unit(),
			_timeline.track_height()
			);
	}

	void set_selected (bool s) {
		_selected = s;
		_timeline.force_redraw (bbox ());
	}
	
	bool selected () const {
		return _selected;
	}

	virtual wxString type () const = 0;
	virtual wxColour colour () const = 0;
	
private:
	
	int y_pos (int t) const
	{
		return _timeline.tracks_position().y + t * _timeline.track_height();
	}

	boost::weak_ptr<const Content> _content;
	int _track;
	bool _selected;
};

class AudioContentView : public ContentView
{
public:
	AudioContentView (Timeline& tl, shared_ptr<const Content> c, int t)
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
	VideoContentView (Timeline& tl, shared_ptr<const Content> c, int t)
		: ContentView (tl, c, t)
	{}

private:	

	wxString type () const
	{
		return _("video");
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

	void paint (wxGraphicsContext* gc)
	{
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));
#else		    
		gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxSOLID));
#endif		    
		
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

	Rect bbox () const
	{
		return Rect (0, _y - 4, _timeline.width(), 24);
	}

private:
	int _y;
};

Timeline::Timeline (wxWindow* parent, shared_ptr<const Film> film)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _film (film)
	, _pixels_per_time_unit (0)
{
	SetDoubleBuffered (true);

	setup_pixels_per_time_unit ();
	
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (Timeline::paint), 0, this);
	Connect (wxID_ANY, wxEVT_LEFT_DOWN, wxMouseEventHandler (Timeline::left_down), 0, this);
	Connect (wxID_ANY, wxEVT_SIZE, wxSizeEventHandler (Timeline::resized), 0, this);

	SetMinSize (wxSize (640, tracks() * track_height() + 96));

	playlist_changed ();

	film->playlist()->Changed.connect (bind (&Timeline::playlist_changed, this));
	film->playlist()->ContentChanged.connect (bind (&Timeline::playlist_changed, this));
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
			_views.push_back (shared_ptr<View> (new AudioContentView (*this, *i, 1)));
		}
	}

	_views.push_back (shared_ptr<View> (new TimeAxisView (*this, tracks() * track_height() + 32)));
		
	Refresh ();
}

int
Timeline::tracks () const
{
	/* XXX */
	return 2;
}

void
Timeline::setup_pixels_per_time_unit ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_pixels_per_time_unit = static_cast<double>(width() - x_offset() * 2) / film->length();
}

void
Timeline::left_down (wxMouseEvent& ev)
{
	list<shared_ptr<View> >::iterator i = _views.begin();
	Position const p (ev.GetX(), ev.GetY());
	while (i != _views.end() && !(*i)->bbox().contains (p)) {
		++i;
	}

	for (list<shared_ptr<View> >::iterator j = _views.begin(); j != _views.end(); ++j) {
		shared_ptr<ContentView> cv = dynamic_pointer_cast<ContentView> (*j);
		if (cv) {
			cv->set_selected (i == j);
		}
	}
}

void
Timeline::force_redraw (Rect const & r)
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

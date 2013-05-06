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
#include "timeline.h"
#include "wx_util.h"
#include "playlist.h"

using std::list;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::bind;

int const Timeline::_track_height = 64;

Timeline::Timeline (wxWindow* parent, shared_ptr<Playlist> pl)
	: wxPanel (parent)
	, _playlist (pl)
{
	SetDoubleBuffered (true);
	
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (Timeline::paint), 0, this);

	if (pl->audio_from() == Playlist::AUDIO_FFMPEG) {
		SetMinSize (wxSize (640, _track_height * 2 + 96));
	} else {
		SetMinSize (wxSize (640, _track_height * (max (size_t (1), pl->audio().size()) + 1) + 96));
	}

	pl->Changed.connect (bind (&Timeline::playlist_changed, this));
	pl->ContentChanged.connect (bind (&Timeline::playlist_changed, this));
}

template <class T>
int
plot_content_list (
	list<shared_ptr<const T> > content, wxGraphicsContext* gc, int x, int y, double pixels_per_second, int track_height, wxString type, bool consecutive
	)
{
	Time t = 0;
	for (typename list<shared_ptr<const T> >::iterator i = content.begin(); i != content.end(); ++i) {
		Time const len = (*i)->temporal_length ();
		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (x + t * pixels_per_second, y);
		path.AddLineToPoint (x + (t + len) * pixels_per_second, y);
		path.AddLineToPoint (x + (t + len) * pixels_per_second, y + track_height);
		path.AddLineToPoint (x + t * pixels_per_second, y + track_height);
		path.AddLineToPoint (x + t * pixels_per_second, y);
		gc->StrokePath (path);
		gc->FillPath (path);

		wxString name = wxString::Format ("%s [%s]", std_to_wx ((*i)->file().filename().string()), type);
		wxDouble name_width;
		wxDouble name_height;
		wxDouble name_descent;
		wxDouble name_leading;
		gc->GetTextExtent (name, &name_width, &name_height, &name_descent, &name_leading);
		
		gc->Clip (wxRegion (x + t * pixels_per_second, y, len * pixels_per_second, track_height));
		gc->DrawText (name, t * pixels_per_second + 12, y + track_height - name_height - 4);
		gc->ResetClip ();

		if (consecutive) {
			t += len;
		} else {
			y += track_height;
		}
	}

	if (consecutive) {
		y += track_height;
	}

	return y;
}

void
Timeline::paint (wxPaintEvent &)
{
	wxPaintDC dc (this);

	shared_ptr<Playlist> pl = _playlist.lock ();
	if (!pl) {
		return;
	}

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int const x_offset = 8;
	int y = 8;
	int const width = GetSize().GetWidth();
	double const pixels_per_second = (width - x_offset * 2) / (pl->content_length() / pl->video_frame_rate());

	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT));

	gc->SetPen (*wxBLACK_PEN);
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 4, wxPENSTYLE_SOLID));
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (149, 121, 232, 255), wxBRUSHSTYLE_SOLID));
#else			
	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 4, SOLID));
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (149, 121, 232, 255), SOLID));
#endif
	y = plot_content_list (pl->video (), gc, x_offset, y, pixels_per_second, _track_height, _("video"), true);
	
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (242, 92, 120, 255), wxBRUSHSTYLE_SOLID));
#else			
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (242, 92, 120, 255), SOLID));
#endif
	y = plot_content_list (pl->audio (), gc, x_offset, y, pixels_per_second, _track_height, _("audio"), pl->audio_from() == Playlist::AUDIO_FFMPEG);

	/* Time axis */

#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
        gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));
#else		    
	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, SOLID));
#endif		    
		    
	int mark_interval = rint (128 / pixels_per_second);
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
	path.MoveToPoint (x_offset, y + 40);
	path.AddLineToPoint (width, y + 40);
	gc->StrokePath (path);

	double t = 0;
	while ((t * pixels_per_second) < width) {
		wxGraphicsPath path = gc->CreatePath ();
		path.MoveToPoint (x_offset + t * pixels_per_second, y + 36);
		path.AddLineToPoint (x_offset + t * pixels_per_second, y + 44);
		gc->StrokePath (path);

		int tc = t;
		int const h = tc / 3600;
		tc -= h * 3600;
		int const m = tc / 60;
		tc -= m * 60;
		int const s = tc;

		wxString str = wxString::Format ("%02d:%02d:%02d", h, m, s);
		wxDouble str_width;
		wxDouble str_height;
		wxDouble str_descent;
		wxDouble str_leading;
		gc->GetTextExtent (str, &str_width, &str_height, &str_descent, &str_leading);

		int const tx = x_offset + t * pixels_per_second;
		if ((tx + str_width) < width) {
			gc->DrawText (str, x_offset + t * pixels_per_second, y + 60);
		}
		t += mark_interval;
	}

	delete gc;
}

void
Timeline::playlist_changed ()
{
	Refresh ();
}

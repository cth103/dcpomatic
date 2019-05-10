/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "simple_video_view.h"
#include "film_viewer.h"
#include "lib/image.h"
#include <dcp/util.h>
#include <wx/wx.h>
#include <boost/bind.hpp>

using std::max;

SimpleVideoView::SimpleVideoView (FilmViewer* viewer, wxWindow* parent)
	: VideoView (viewer)
{
	_panel = new wxPanel (parent);

#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif

	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
	_panel->SetBackgroundColour (*wxBLACK);

	_panel->Bind (wxEVT_PAINT, boost::bind (&SimpleVideoView::paint, this));
	_panel->Bind (wxEVT_SIZE, boost::bind(boost::ref(Sized)));
}

void
SimpleVideoView::paint ()
{
	wxPaintDC dc (_panel);

#ifdef DCPOMATIC_VARIANT_SWAROOP
	if (viewer->background_image()) {
		dc.Clear ();
		maybe_draw_background_image (dc);
		return;
	}
#endif

	dcp::Size const out_size = _viewer->out_size ();
	wxSize const panel_size = _panel->GetSize ();

	if (!out_size.width || !out_size.height || !_image || out_size != _image->size()) {
		dc.Clear ();
	} else {

		wxImage frame (out_size.width, out_size.height, _image->data()[0], true);
		wxBitmap frame_bitmap (frame);
		dc.DrawBitmap (frame_bitmap, 0, max(0, (panel_size.GetHeight() - out_size.height) / 2));

#ifdef DCPOMATIC_VARIANT_SWAROOP
		XXX
		DCPTime const period = DCPTime::from_seconds(Config::instance()->player_watermark_period() * 60);
		int64_t n = _video_position.get() / period.get();
		DCPTime from(n * period.get());
		DCPTime to = from + DCPTime::from_seconds(Config::instance()->player_watermark_duration() / 1000.0);
		if (from <= _video_position && _video_position <= to) {
			if (!_in_watermark) {
				_in_watermark = true;
				_watermark_x = rand() % _panel_size.width;
				_watermark_y = rand() % _panel_size.height;
			}
			dc.SetTextForeground(*wxWHITE);
			string wm = Config::instance()->player_watermark_theatre();
			boost::posix_time::ptime t = boost::posix_time::second_clock::local_time();
			wm += "\n" + boost::posix_time::to_iso_extended_string(t);
			dc.DrawText(std_to_wx(wm), _watermark_x, _watermark_y);
		} else {
			_in_watermark = false;
		}
#endif
	}

	if (out_size.width < panel_size.GetWidth()) {
		/* XXX: these colours are right for GNOME; may need adjusting for other OS */
		wxPen   p (_viewer->pad_black() ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		wxBrush b (_viewer->pad_black() ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (out_size.width, 0, panel_size.GetWidth() - out_size.width, panel_size.GetHeight());
	}

	if (out_size.height < panel_size.GetHeight()) {
		wxPen   p (_viewer->pad_black() ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		wxBrush b (_viewer->pad_black() ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		dc.SetPen (p);
		dc.SetBrush (b);
		int const gap = (panel_size.GetHeight() - out_size.height) / 2;
		dc.DrawRectangle (0, 0, panel_size.GetWidth(), gap);
		dc.DrawRectangle (0, gap + out_size.height + 1, panel_size.GetWidth(), gap + 1);
	}

	if (_viewer->outline_content()) {
		Position<int> inter_position = _viewer->inter_position ();
		dcp::Size inter_size = _viewer->inter_size ();
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (inter_position.x, inter_position.y + (panel_size.GetHeight() - out_size.height) / 2, inter_size.width, inter_size.height);
	}
}

void
SimpleVideoView::update ()
{
	_panel->Refresh ();
	_panel->Update ();
}

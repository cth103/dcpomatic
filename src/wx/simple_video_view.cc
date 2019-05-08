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

SimpleVideoView::SimpleVideoView (wxWindow* parent)
{
	_panel = new wxPanel (parent);

#ifndef __WXOSX__
	_panel->SetDoubleBuffered (true);
#endif

	_panel->SetBackgroundStyle (wxBG_STYLE_PAINT);
	_panel->SetBackgroundColour (*wxBLACK);

	_panel->Bind (wxEVT_PAINT, boost::bind (&SimpleVideoView::paint, this));
}

void
SimpleVideoView::paint ()
{
	wxPaintDC dc (_panel);

#ifdef DCPOMATIC_VARIANT_SWAROOP
	if (_background_image) {
		dc.Clear ();
		maybe_draw_background_image (dc);
		_state_timer.unset ();
		return;
	}
#endif

	if (!_out_size.width || !_out_size.height || !_film || !_frame || _out_size != _frame->size()) {
		dc.Clear ();
	} else {

		wxImage frame (_out_size.width, _out_size.height, _frame->data()[0], true);
		wxBitmap frame_bitmap (frame);
		dc.DrawBitmap (frame_bitmap, 0, max(0, (_panel_size.height - _out_size.height) / 2));

#ifdef DCPOMATIC_VARIANT_SWAROOP
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

	if (_out_size.width < _panel_size.width) {
		/* XXX: these colours are right for GNOME; may need adjusting for other OS */
		wxPen   p (_pad_black ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		wxBrush b (_pad_black ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle (_out_size.width, 0, _panel_size.width - _out_size.width, _panel_size.height);
	}

	if (_out_size.height < _panel_size.height) {
		wxPen   p (_pad_black ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		wxBrush b (_pad_black ? wxColour(0, 0, 0) : wxColour(240, 240, 240));
		dc.SetPen (p);
		dc.SetBrush (b);
		int const gap = (_panel_size.height - _out_size.height) / 2;
		dc.DrawRectangle (0, 0, _panel_size.width, gap);
		dc.DrawRectangle (0, gap + _out_size.height + 1, _panel_size.width, gap + 1);
	}

	if (_outline_content) {
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (_inter_position.x, _inter_position.y + (_panel_size.height - _out_size.height) / 2, _inter_size.width, _inter_size.height);
	}
}

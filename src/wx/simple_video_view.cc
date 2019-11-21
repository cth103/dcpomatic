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
#include "wx_util.h"
#include "closed_captions_dialog.h"
#include "lib/image.h"
#include "lib/dcpomatic_log.h"
#include "lib/butler.h"
#include <dcp/util.h>
#include <wx/wx.h>
#include <boost/bind.hpp>

using std::max;
using std::string;
using boost::optional;
using boost::shared_ptr;
using namespace dcpomatic;

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

	_timer.Bind (wxEVT_TIMER, boost::bind(&SimpleVideoView::timer, this));
}

void
SimpleVideoView::paint ()
{
        _state_timer.set("paint-panel");
	wxPaintDC dc (_panel);

	dcp::Size const out_size = _viewer->out_size ();
	wxSize const panel_size = _panel->GetSize ();

#ifdef DCPOMATIC_VARIANT_SWAROOP
	if (_viewer->background_image()) {
		dc.Clear ();
		optional<boost::filesystem::path> bg = Config::instance()->player_background_image();
		if (bg) {
			wxImage image (std_to_wx(bg->string()));
			wxBitmap bitmap (image);
			dc.DrawBitmap (bitmap, max(0, (panel_size.GetWidth() - image.GetSize().GetWidth()) / 2), max(0, (panel_size.GetHeight() - image.GetSize().GetHeight()) / 2));
		}
		return;
	}
#endif

	if (!out_size.width || !out_size.height || !_image || out_size != _image->size()) {
		dc.Clear ();
	} else {

		wxImage frame (out_size.width, out_size.height, _image->data()[0], true);
		wxBitmap frame_bitmap (frame);
		dc.DrawBitmap (frame_bitmap, 0, max(0, (panel_size.GetHeight() - out_size.height) / 2));

#ifdef DCPOMATIC_VARIANT_SWAROOP
		DCPTime const period = DCPTime::from_seconds(Config::instance()->player_watermark_period() * 60);
		int64_t n = _viewer->position().get() / period.get();
		DCPTime from(n * period.get());
		DCPTime to = from + DCPTime::from_seconds(Config::instance()->player_watermark_duration() / 1000.0);
		if (from <= _viewer->position() && _viewer->position() <= to) {
			if (!_in_watermark) {
				_in_watermark = true;
				_watermark_x = rand() % panel_size.GetWidth();
				_watermark_y = rand() % panel_size.GetHeight();
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
		wxPen p (wxColour (255, 0, 0), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (_inter_position.x, _inter_position.y + (panel_size.GetHeight() - out_size.height) / 2, _inter_size.width, _inter_size.height);
	}
        _state_timer.unset();
}

void
SimpleVideoView::update ()
{
	_state_timer.set ("update-view");
	_panel->Refresh ();
	_panel->Update ();
	_state_timer.unset ();
}

void
SimpleVideoView::timer ()
{
	if (!_viewer->playing()) {
		return;
	}

	display_next_frame (false);
	DCPTime const next = _viewer->position() + _viewer->one_video_frame();

	if (next >= length()) {
		_viewer->stop ();
		_viewer->Finished ();
		return;
	}

	LOG_DEBUG_PLAYER("%1 -> %2; delay %3", next.seconds(), _viewer->time().seconds(), max((next.seconds() - _viewer->time().seconds()) * 1000, 1.0));
	_timer.Start (time_until_next_frame(), wxTIMER_ONE_SHOT);

	if (_viewer->butler()) {
		_viewer->butler()->rethrow ();
	}
}

void
SimpleVideoView::start ()
{
	VideoView::start ();
	timer ();
}

/** Try to get a frame from the butler and display it.
 *  @param non_blocking true to return false quickly if no video is available quickly (i.e. we are waiting for the butler).
 *  false to ask the butler to block until it has video (unless it is suspended).
 *  @return true on success, false if we did nothing because it would have taken too long.
 */
bool
SimpleVideoView::display_next_frame (bool non_blocking)
{
	bool r = get_next_frame (non_blocking);
	if (!r) {
		if (non_blocking) {
			/* No video available; return saying we failed */
			return false;
		} else {
			/* Player was suspended; come back later */
			signal_manager->when_idle (boost::bind(&SimpleVideoView::display_next_frame, this, false));
			return false;
		}
	}

	display_player_video ();

	try {
		_viewer->butler()->rethrow ();
	} catch (DecodeError& e) {
		error_dialog (get(), e.what());
	}

	return true;
}

void
SimpleVideoView::display_player_video ()
{
	if (!player_video().first) {
		set_image (shared_ptr<Image>());
		update ();
		return;
	}

	if (_viewer->playing() && (_viewer->time() - player_video().second) > one_video_frame()) {
		/* Too late; just drop this frame before we try to get its image (which will be the time-consuming
		   part if this frame is J2K).
		*/
		add_dropped ();
		return;
	}

	/* In an ideal world, what we would do here is:
	 *
	 * 1. convert to XYZ exactly as we do in the DCP creation path.
	 * 2. convert back to RGB for the preview display, compensating
	 *    for the monitor etc. etc.
	 *
	 * but this is inefficient if the source is RGB.  Since we don't
	 * (currently) care too much about the precise accuracy of the preview's
	 * colour mapping (and we care more about its speed) we try to short-
	 * circuit this "ideal" situation in some cases.
	 *
	 * The content's specified colour conversion indicates the colourspace
	 * which the content is in (according to the user).
	 *
	 * PlayerVideo::image (bound to PlayerVideo::force) will take the source
	 * image and convert it (from whatever the user has said it is) to RGB.
	 */

	_state_timer.set ("get image");

	set_image (
		player_video().first->image(bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true)
		);

	_state_timer.set ("ImageChanged");
	_viewer->ImageChanged (player_video().first);
	_state_timer.unset ();

	_inter_position = player_video().first->inter_position ();
	_inter_size = player_video().first->inter_size ();

	update ();

	_viewer->closed_captions_dialog()->update (_viewer->time());
}

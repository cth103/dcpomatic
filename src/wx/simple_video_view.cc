/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "closed_captions_dialog.h"
#include "film_viewer.h"
#include "simple_video_view.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/dcpomatic_log.h"
#include "lib/image.h"
#include "lib/video_filter_graph.h"
#include "lib/video_filter_graph_set.h"
#include <dcp/util.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


using std::max;
using std::shared_ptr;
using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


SimpleVideoView::SimpleVideoView (FilmViewer* viewer, wxWindow* parent)
	: VideoView (viewer)
	, _rec2020_filter("convert", "convert", "", "colorspace=all=bt709:iall=bt2020")
	, _rec2020_filter_graph({ _rec2020_filter }, dcp::Fraction(24, 1))
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
	auto scale = 1 / dpi_scale_factor (_panel);
	dc.SetLogicalScale (scale, scale);

	auto const panel_size = dcp::Size(_panel->GetSize().GetWidth() / scale, _panel->GetSize().GetHeight() / scale);
	auto pad = pad_colour();

	dcp::Size out_size;
	if (!_image) {
		wxBrush b (pad);
		dc.SetBackground (b);
		dc.Clear ();
	} else {
		DCPOMATIC_ASSERT (_image->alignment() == Image::Alignment::COMPACT);
		out_size = _image->size();
		wxImage frame (out_size.width, out_size.height, _image->data()[0], true);
		wxBitmap frame_bitmap (frame);
		dc.DrawBitmap(frame_bitmap, 0, max(0, (panel_size.height - out_size.height) / 2));
	}

	if (out_size.width < panel_size.width) {
		wxPen   p (pad);
		wxBrush b (pad);
		dc.SetPen (p);
		dc.SetBrush (b);
		dc.DrawRectangle(out_size.width, 0, panel_size.width - out_size.width, panel_size.height);
	}

	if (out_size.height < panel_size.height) {
		wxPen   p (pad);
		wxBrush b (pad);
		dc.SetPen (p);
		dc.SetBrush (b);
		int const gap = (panel_size.height - out_size.height) / 2;
		dc.DrawRectangle(0, 0, panel_size.width, gap);
		dc.DrawRectangle(0, gap + out_size.height, panel_size.width, gap + 1);
	}

	if (_viewer->outline_content()) {
		wxPen p (outline_content_colour(), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle(_inter_position.x, _inter_position.y + (panel_size.height - out_size.height) / 2, _inter_size.width, _inter_size.height);
	}

	auto subs = _viewer->outline_subtitles();
	if (subs) {
		wxPen p (outline_subtitles_colour(), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle (subs->x * out_size.width, subs->y * out_size.height, subs->width * out_size.width, subs->height * out_size.height);
	}

	if (_viewer->crop_guess()) {
		wxPen p (crop_guess_colour(), 2);
		dc.SetPen (p);
		dc.SetBrush (*wxTRANSPARENT_BRUSH);
		auto const crop_guess = _viewer->crop_guess().get();
		dc.DrawRectangle (
			_inter_position.x + _inter_size.width * crop_guess.x,
			_inter_position.y + _inter_size.height * crop_guess.y,
			_inter_size.width * crop_guess.width,
			_inter_size.height * crop_guess.height
			);
	}

        _state_timer.unset();
}


void
SimpleVideoView::refresh_panel ()
{
	_state_timer.set ("refresh-panel");
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
	auto const next = position() + _viewer->one_video_frame();

	if (next >= length()) {
		_viewer->finished ();
		return;
	}

	LOG_DEBUG_VIDEO_VIEW("{} -> {}; delay {}", next.seconds(), _viewer->time().seconds(), max((next.seconds() - _viewer->time().seconds()) * 1000, 1.0));
	_timer.Start (max(1, time_until_next_frame().get_value_or(0)), wxTIMER_ONE_SHOT);

	if (_viewer->butler()) {
		try {
			_viewer->butler()->rethrow();
		} catch (dcp::FileError& e) {
			error_dialog(_panel, _("Could not play content"), std_to_wx(e.what()));
		}
	}

	LOG_DEBUG_PLAYER("Latency {}", _viewer->average_latency());
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
VideoView::NextFrameResult
SimpleVideoView::display_next_frame (bool non_blocking)
{
	auto const r = get_next_frame (non_blocking);
	if (r != SUCCESS) {
		return r;
	}

	update ();

	try {
		_viewer->butler()->rethrow ();
	} catch (DecodeError& e) {
		error_dialog(get(), std_to_wx(e.what()));
	}

	return SUCCESS;
}


void
SimpleVideoView::update ()
{
	if (!player_video().first) {
		_image.reset ();
		refresh_panel ();
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
	 * PlayerVideo::image will take the source image and convert it
	 * (from whatever the user has said it is) to RGB.
	 */

	_state_timer.set ("get image");

	auto const pv = player_video();
	_image = pv.first->image(AV_PIX_FMT_RGB24, VideoRange::FULL, true);
	if (pv.first->colour_conversion() && pv.first->colour_conversion()->about_equal(dcp::ColourConversion::rec2020_to_xyz(), 1e-6)) {
		_image = Image::ensure_alignment(_rec2020_filter_graph.get(_image->size(), _image->pixel_format())->process(_image).front(), Image::Alignment::COMPACT);
	}

	_state_timer.set ("ImageChanged");
	_viewer->image_changed (player_video().first);
	_state_timer.unset ();

	_inter_position = player_video().first->inter_position ();
	_inter_size = player_video().first->inter_size ();

	refresh_panel ();
}

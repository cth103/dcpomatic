/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "film_viewer.h"
#include "video_waveform_plot.h"
#include "wx_util.h"
#include "lib/dcp_video.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/player_video.h"
#include <dcp/locale_convert.h>
#include <dcp/openjpeg_image.h>
#include <wx/rawbmp.h>
#include <wx/graphics.h>
#include <boost/bind/bind.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::max;
using std::min;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;


int const VideoWaveformPlot::_vertical_margin = 8;
int const VideoWaveformPlot::_pixel_values = 4096;
int const VideoWaveformPlot::_x_axis_width = 52;


VideoWaveformPlot::VideoWaveformPlot (wxWindow* parent, weak_ptr<const Film> film, weak_ptr<FilmViewer> viewer)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _film (film)
{
#ifndef __WXOSX__
	SetDoubleBuffered (true);
#endif

	auto fv = viewer.lock ();
	DCPOMATIC_ASSERT (fv);

	_viewer_connection = fv->ImageChanged.connect (boost::bind(&VideoWaveformPlot::set_image, this, _1));

	Bind (wxEVT_PAINT, boost::bind(&VideoWaveformPlot::paint, this));
	Bind (wxEVT_SIZE,  boost::bind(&VideoWaveformPlot::sized, this, _1));
	Bind (wxEVT_MOTION, boost::bind(&VideoWaveformPlot::mouse_moved, this, _1));

	SetMinSize (wxSize (640, 512));
	SetBackgroundColour (wxColour (0, 0, 0));
}


void
VideoWaveformPlot::paint ()
{
	wxPaintDC dc (this);

	if (_dirty) {
		create_waveform ();
		_dirty = false;
	}

	if (!_waveform) {
		return;
	}

	auto gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int const height = _waveform->size().height;

	gc->SetPen (wxPen (wxColour (255, 255, 255), 1, wxPENSTYLE_SOLID));

	gc->SetFont (gc->CreateFont (*wxSMALL_FONT, wxColour (255, 255, 255)));
	double label_width;
	double label_height;
	double label_descent;
	double label_leading;
	gc->GetTextExtent (wxT ("1024"), &label_width, &label_height, &label_descent, &label_leading);

	double extra[3];
	double w;
	gc->GetTextExtent (wxT ("0"), &w, &label_height, &label_descent, &label_leading);
	extra[0] = label_width - w;
	gc->GetTextExtent (wxT ("64"), &w, &label_height, &label_descent, &label_leading);
	extra[1] = label_width - w;
	gc->GetTextExtent (wxT ("512"), &w, &label_height, &label_descent, &label_leading);
	extra[2] = label_width - w;

	int label_gaps = 2;
	while (height / label_gaps > 64) {
		label_gaps *= 2;
	}

	for (int i = 0; i < label_gaps + 1; ++i) {
		auto p = gc->CreatePath ();
		int const y = _vertical_margin + height - (i * height / label_gaps) - 1;
		p.MoveToPoint (label_width + 8, y);
		p.AddLineToPoint (_x_axis_width - 4, y);
		gc->StrokePath (p);
		int x = 4;
		int const n = i * _pixel_values / label_gaps;
		if (n < 10) {
			x += extra[0];
		} else if (n < 100) {
			x += extra[1];
		} else if (n < 1000) {
			x += extra[2];
		}
		gc->DrawText (std_to_wx(locale_convert<string>(n)), x, y - (label_height / 2));
	}

	wxImage waveform (_waveform->size().width, height, _waveform->data()[0], true);
	wxBitmap bitmap (waveform);
	gc->DrawBitmap (bitmap, _x_axis_width, _vertical_margin, _waveform->size().width, height);

	delete gc;
}


void
VideoWaveformPlot::create_waveform ()
{
	_waveform.reset ();

	if (!_image) {
		return;
	}

	auto const image_size = _image->size();
	int const waveform_height = GetSize().GetHeight() - _vertical_margin * 2;
	_waveform = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size (image_size.width, waveform_height), true);

	for (int x = 0; x < image_size.width; ++x) {

		/* Work out one vertical `slice' of waveform pixels.  Each value in
		   strip is the number of samples in image with the corresponding group of
		   values.
		*/
		int strip[waveform_height];
		memset (strip, 0, waveform_height * sizeof(int));

		int* ip = _image->data (_component) + x;
		for (int y = 0; y < image_size.height; ++y) {
			strip[*ip * waveform_height / _pixel_values]++;
			ip += image_size.width;
		}

		/* Copy slice into the waveform */
		uint8_t* wp = _waveform->data()[0] + x * 3;
		for (int y = waveform_height - 1; y >= 0; --y) {
			wp[0] = wp[1] = wp[2] = min (255, (strip[y] * 255 / waveform_height) * _contrast);
			wp += _waveform->stride()[0];
		}
	}

	_waveform = _waveform->scale (
		dcp::Size (GetSize().GetWidth() - _x_axis_width, waveform_height),
		dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB24, false, false
		);
}


void
VideoWaveformPlot::set_image (shared_ptr<PlayerVideo> image)
{
	if (!_enabled) {
		return;
	}

	/* We must copy the PlayerVideo here as we will call ::image() on it, potentially
	   with a different pixel_format than was used when ::prepare() was called.
	*/
	_image = DCPVideo::convert_to_xyz (image->shallow_copy(), [](dcp::NoteType, string) {});
	_dirty = true;
	Refresh ();
}


void
VideoWaveformPlot::sized (wxSizeEvent &)
{
	_dirty = true;
}


void
VideoWaveformPlot::set_enabled (bool e)
{
	_enabled = e;
}


void
VideoWaveformPlot::set_component (int c)
{
	_component = c;
	_dirty = true;
	Refresh ();
}


/** Set `contrast', i.e. a fudge multiplication factor to make low-level signals easier to see,
 *  between 0 and 256.
 */
void
VideoWaveformPlot::set_contrast (int b)
{
	_contrast = b;
	_dirty = true;
	Refresh ();
}


void
VideoWaveformPlot::mouse_moved (wxMouseEvent& ev)
{
	if (!_image) {
		return;
	}

	if (_dirty) {
		create_waveform ();
		_dirty = false;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	auto const full = film->frame_size ();

	auto const xs = static_cast<double> (full.width) / _waveform->size().width;
	int const x1 = max (0, min (full.width - 1, int (floor (ev.GetPosition().x - _x_axis_width - 0.5) * xs)));
	int const x2 = max (0, min (full.width - 1, int (floor (ev.GetPosition().x - _x_axis_width + 0.5) * xs)));

	auto const ys = static_cast<double> (_pixel_values) / _waveform->size().height;
	int const fy = _waveform->size().height - (ev.GetPosition().y - _vertical_margin);
	int const y1 = max (0, min (_pixel_values - 1, int (floor (fy - 0.5) * ys)));
	int const y2 = max (0, min (_pixel_values - 1, int (floor (fy + 0.5) * ys)));

	MouseMoved (x1, x2, y1, y2);
}

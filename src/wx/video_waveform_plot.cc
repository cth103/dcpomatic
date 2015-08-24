/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "video_waveform_plot.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "lib/image.h"
#include "lib/raw_convert.h"
#include "lib/dcp_video.h"
#include <dcp/openjpeg_image.h>
#include <wx/rawbmp.h>
#include <wx/graphics.h>
#include <boost/bind.hpp>

using std::cout;
using std::min;
using std::string;
using boost::weak_ptr;
using boost::shared_ptr;

int const VideoWaveformPlot::_vertical_margin = 8;

VideoWaveformPlot::VideoWaveformPlot (wxWindow* parent, FilmViewer* viewer)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _dirty (true)
	, _enabled (false)
	, _component (0)
	, _contrast (0)
{
#ifndef __WXOSX__
	SetDoubleBuffered (true);
#endif

	_viewer_connection = viewer->ImageChanged.connect (boost::bind (&VideoWaveformPlot::set_image, this, _1));

	Bind (wxEVT_PAINT, boost::bind (&VideoWaveformPlot::paint, this));
	Bind (wxEVT_SIZE,  boost::bind (&VideoWaveformPlot::sized, this, _1));

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

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int const axis_x = 48;
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
		wxGraphicsPath p = gc->CreatePath ();
		int const y = _vertical_margin + height - (i * height / label_gaps) - 1;
		p.MoveToPoint (label_width + 8, y);
		p.AddLineToPoint (axis_x, y);
		gc->StrokePath (p);
		int x = 4;
		int const n = i * 4096 / label_gaps;
		if (n < 10) {
			x += extra[0];
		} else if (n < 100) {
			x += extra[1];
		} else if (n < 1000) {
			x += extra[2];
		}
		gc->DrawText (std_to_wx (raw_convert<string> (n)), x, y - (label_height / 2));
	}

	wxImage waveform (_waveform->size().width, height, _waveform->data()[0], true);
	wxBitmap bitmap (waveform);
	gc->DrawBitmap (bitmap, axis_x + 4, _vertical_margin, _waveform->size().width, height);

	delete gc;
}

void
VideoWaveformPlot::create_waveform ()
{
	_waveform.reset ();

	if (!_image) {
		return;
	}

	dcp::Size const size = _image->size();
	_waveform.reset (new Image (PIX_FMT_RGB24, dcp::Size (size.width, size.height), true));

	for (int x = 0; x < size.width; ++x) {

		/* Work out one vertical `slice' of waveform pixels.  Each value in
		   strip is the number of samples in image with the corresponding group of
		   values.
		*/
		int strip[size.height];
		for (int i = 0; i < size.height; ++i) {
			strip[i] = 0;
		}

		int* ip = _image->data (_component) + x;
		for (int y = 0; y < size.height; ++y) {
			strip[*ip * size.height / 4096]++;
			ip += size.width;
		}

		/* Copy slice into the waveform */
		uint8_t* wp = _waveform->data()[0] + x * 3;
		for (int y = size.height - 1; y >= 0; --y) {
			wp[0] = wp[1] = wp[2] = min (255, (strip[y] * 255 / size.height) * _contrast);
			wp += _waveform->stride()[0];
		}
	}

	_waveform = _waveform->scale (
		dcp::Size (GetSize().GetWidth() - 32, GetSize().GetHeight() - _vertical_margin * 2),
		dcp::YUV_TO_RGB_REC709, PIX_FMT_RGB24, false
		);
}

static void
note ()
{

}

void
VideoWaveformPlot::set_image (weak_ptr<PlayerVideo> image)
{
	if (!_enabled) {
		return;
	}

	shared_ptr<PlayerVideo> pv = image.lock ();
	_image = DCPVideo::convert_to_xyz (pv, boost::bind (&note));
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

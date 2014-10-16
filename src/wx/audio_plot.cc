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

#include <iostream>
#include <boost/bind.hpp>
#include <wx/graphics.h>
#include "audio_plot.h"
#include "lib/audio_decoder.h"
#include "lib/audio_analysis.h"
#include "wx/wx_util.h"

using std::cout;
using std::vector;
using std::list;
using std::max;
using std::min;
using boost::bind;
using boost::shared_ptr;

int const AudioPlot::_minimum = -70;
int const AudioPlot::max_smoothing = 128;

AudioPlot::AudioPlot (wxWindow* parent)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _gain (0)
	, _smoothing (max_smoothing / 2)
{
#ifndef __WXOSX__	
	SetDoubleBuffered (true);
#endif	

	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_visible[i] = false;
	}

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_visible[i] = false;
	}

	_colours.push_back (wxColour (	0,   0,	  0));
	_colours.push_back (wxColour (255,   0,	  0));
	_colours.push_back (wxColour (	0, 255,	  0));
	_colours.push_back (wxColour (139,   0, 204));
	_colours.push_back (wxColour (	0,   0, 255));
	_colours.push_back (wxColour (  0, 139,   0));
	_colours.push_back (wxColour (  0,   0, 139));
	_colours.push_back (wxColour (255, 255,   0));
	_colours.push_back (wxColour (  0, 255, 255));
	_colours.push_back (wxColour (255,   0, 255));
	_colours.push_back (wxColour (255,   0, 139));
	_colours.push_back (wxColour (139,   0, 255));

	set_analysis (shared_ptr<AudioAnalysis> ());

#if MAX_DCP_AUDIO_CHANNELS != 12
#warning AudioPlot::AudioPlot is expecting the wrong MAX_DCP_AUDIO_CHANNELS
#endif	
	
	Bind (wxEVT_PAINT, boost::bind (&AudioPlot::paint, this));
	
	SetMinSize (wxSize (640, 512));
}

void
AudioPlot::set_analysis (shared_ptr<AudioAnalysis> a)
{
	_analysis = a;

	if (!a) {
		_message = _("Please wait; audio is being analysed...");
	}

	Refresh ();
}

void
AudioPlot::set_channel_visible (int c, bool v)
{
	_channel_visible[c] = v;
	Refresh ();
}

void
AudioPlot::set_type_visible (int t, bool v)
{
	_type_visible[t] = v;
	Refresh ();
}

void
AudioPlot::set_message (wxString s)
{
	_message = s;
	Refresh ();
}

struct Metrics
{
	double db_label_width;
	int height;
	int y_origin;
	float x_scale;
	float y_scale;
};

void
AudioPlot::paint ()
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	if (!_analysis || _analysis->channels() == 0) {
		gc->SetFont (gc->CreateFont (*wxNORMAL_FONT));
		gc->DrawText (_message, 32, 32);
		return;
	}

	wxGraphicsPath grid = gc->CreatePath ();
	gc->SetFont (gc->CreateFont (*wxSMALL_FONT));
	wxDouble db_label_height;
	wxDouble db_label_descent;
	wxDouble db_label_leading;
	Metrics metrics;
	gc->GetTextExtent (wxT ("-80dB"), &metrics.db_label_width, &db_label_height, &db_label_descent, &db_label_leading);

	metrics.db_label_width += 8;
	
	int const data_width = GetSize().GetWidth() - metrics.db_label_width;
	/* Assume all channels have the same number of points */
	metrics.x_scale = data_width / float (_analysis->points (0));
	metrics.height = GetSize().GetHeight ();
	metrics.y_origin = 32;
	metrics.y_scale = (metrics.height - metrics.y_origin) / -_minimum;

	for (int i = _minimum; i <= 0; i += 10) {
		int const y = (metrics.height - (i - _minimum) * metrics.y_scale) - metrics.y_origin;
		grid.MoveToPoint (metrics.db_label_width - 4, y);
		grid.AddLineToPoint (metrics.db_label_width + data_width, y);
		gc->DrawText (std_to_wx (String::compose ("%1dB", i)), 0, y - (db_label_height / 2));
	}

	gc->SetPen (wxPen (wxColour (200, 200, 200)));
	gc->StrokePath (grid);

	gc->DrawText (_("Time"), data_width, metrics.height - metrics.y_origin + db_label_height / 2);
	
	if (_type_visible[AudioPoint::PEAK]) {
		for (int c = 0; c < MAX_DCP_AUDIO_CHANNELS; ++c) {
			wxGraphicsPath p = gc->CreatePath ();
			if (_channel_visible[c] && c < _analysis->channels()) {
				plot_peak (p, c, metrics);
			}
			wxColour const col = _colours[c];
			gc->SetPen (wxPen (wxColour (col.Red(), col.Green(), col.Blue(), col.Alpha() / 2), 1, wxPENSTYLE_SOLID));
			gc->StrokePath (p);
		}
	}

	if (_type_visible[AudioPoint::RMS]) {
		for (int c = 0; c < MAX_DCP_AUDIO_CHANNELS; ++c) {
			wxGraphicsPath p = gc->CreatePath ();
			if (_channel_visible[c] && c < _analysis->channels()) {
				plot_rms (p, c, metrics);
			}
			wxColour const col = _colours[c];
			gc->SetPen (wxPen (col, 1, wxPENSTYLE_SOLID));
			gc->StrokePath (p);
		}
	}

	wxGraphicsPath axes = gc->CreatePath ();
	axes.MoveToPoint (metrics.db_label_width, 0);
	axes.AddLineToPoint (metrics.db_label_width, metrics.height - metrics.y_origin);
	axes.AddLineToPoint (metrics.db_label_width + data_width, metrics.height - metrics.y_origin);
	gc->SetPen (wxPen (wxColour (0, 0, 0)));
	gc->StrokePath (axes);

	delete gc;
}

float
AudioPlot::y_for_linear (float p, Metrics const & metrics) const
{
	if (p < 1e-4) {
		p = 1e-4;
	}
	
	return metrics.height - (20 * log10(p) - _minimum + _gain) * metrics.y_scale - metrics.y_origin;
}

void
AudioPlot::plot_peak (wxGraphicsPath& path, int channel, Metrics const & metrics) const
{
	if (_analysis->points (channel) == 0) {
		return;
	}
	
	path.MoveToPoint (metrics.db_label_width, y_for_linear (_analysis->get_point(channel, 0)[AudioPoint::PEAK], metrics));

	float peak = 0;
	int const N = _analysis->points(channel);
	for (int i = 0; i < N; ++i) {
		float const p = _analysis->get_point(channel, i)[AudioPoint::PEAK];
		peak -= 0.01f * (1 - log10 (_smoothing) / log10 (max_smoothing));
		if (p > peak) {
			peak = p;
		} else if (peak < 0) {
			peak = 0;
		}

		path.AddLineToPoint (metrics.db_label_width + i * metrics.x_scale, y_for_linear (peak, metrics));
	}
}

void
AudioPlot::plot_rms (wxGraphicsPath& path, int channel, Metrics const & metrics) const
{
	if (_analysis->points (channel) == 0) {
		return;
	}
	
	path.MoveToPoint (metrics.db_label_width, y_for_linear (_analysis->get_point(channel, 0)[AudioPoint::RMS], metrics));

	list<float> smoothing;

	int const N = _analysis->points(channel);

	float const first = _analysis->get_point(channel, 0)[AudioPoint::RMS];
	float const last = _analysis->get_point(channel, N - 1)[AudioPoint::RMS];

	int const before = _smoothing / 2;
	int const after = _smoothing - before;
	
	/* Pre-load the smoothing list */
	for (int i = 0; i < before; ++i) {
		smoothing.push_back (first);
	}
	for (int i = 0; i < after; ++i) {
		if (i < N) {
			smoothing.push_back (_analysis->get_point(channel, i)[AudioPoint::RMS]);
		} else {
			smoothing.push_back (last);
		}
	}

	for (int i = 0; i < N; ++i) {

		int const next_for_window = i + after;

		if (next_for_window < N) {
			smoothing.push_back (_analysis->get_point(channel, i)[AudioPoint::RMS]);
		} else {
			smoothing.push_back (last);
		}

		smoothing.pop_front ();

		float p = 0;
		for (list<float>::const_iterator j = smoothing.begin(); j != smoothing.end(); ++j) {
			p += pow (*j, 2);
		}

		if (smoothing.size() > 0) {
			p = sqrt (p / smoothing.size ());
		}

		path.AddLineToPoint (metrics.db_label_width + i * metrics.x_scale, y_for_linear (p, metrics));
	}
}

void
AudioPlot::set_gain (float g)
{
	_gain = g;
	Refresh ();
}

void
AudioPlot::set_smoothing (int s)
{
	_smoothing = s;
	Refresh ();
}

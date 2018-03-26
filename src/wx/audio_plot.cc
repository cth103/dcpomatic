/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "audio_plot.h"
#include "lib/audio_decoder.h"
#include "lib/audio_analysis.h"
#include "lib/compose.hpp"
#include "wx/wx_util.h"
#include <wx/graphics.h>
#include <boost/bind.hpp>
#include <iostream>

using std::cout;
using std::vector;
using std::list;
using std::max;
using std::min;
using std::map;
using boost::bind;
using boost::optional;
using boost::shared_ptr;

int const AudioPlot::_minimum = -70;
int const AudioPlot::_cursor_size = 8;
int const AudioPlot::max_smoothing = 128;

AudioPlot::AudioPlot (wxWindow* parent)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
	, _smoothing (max_smoothing / 2)
	, _gain_correction (0)
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

	_colours.push_back (wxColour (139, 139, 255));
	_colours.push_back (wxColour (  0, 139, 255));
	_colours.push_back (wxColour (255, 139, 139));
	_colours.push_back (wxColour (255, 139,   0));

	set_analysis (shared_ptr<AudioAnalysis> ());

#if MAX_DCP_AUDIO_CHANNELS != 16
#warning AudioPlot::AudioPlot is expecting the wrong MAX_DCP_AUDIO_CHANNELS
#endif

	Bind (wxEVT_PAINT, boost::bind (&AudioPlot::paint, this));
	Bind (wxEVT_MOTION, boost::bind (&AudioPlot::mouse_moved, this, _1));
	Bind (wxEVT_LEAVE_WINDOW, boost::bind (&AudioPlot::mouse_leave, this, _1));

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
	float x_scale; ///< pixels per data point
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

	wxGraphicsPath h_grid = gc->CreatePath ();
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
		h_grid.MoveToPoint (metrics.db_label_width - 4, y);
		h_grid.AddLineToPoint (metrics.db_label_width + data_width, y);
		gc->DrawText (std_to_wx (String::compose ("%1dB", i)), 0, y - (db_label_height / 2));
	}

	gc->SetPen (wxPen (wxColour (200, 200, 200)));
	gc->StrokePath (h_grid);

	/* Draw an x axis with marks */

	wxGraphicsPath v_grid = gc->CreatePath ();

	DCPOMATIC_ASSERT (_analysis->samples_per_point() != 0.0);
	double const pps = _analysis->sample_rate() * metrics.x_scale / _analysis->samples_per_point();

	gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));

	double const mark_interval = calculate_mark_interval (rint (128 / pps));

	DCPTime t = DCPTime::from_seconds (mark_interval);
	while ((t.seconds() * pps) < data_width) {
		double tc = t.seconds ();
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

		int const tx = llrintf (metrics.db_label_width + t.seconds() * pps);
		gc->DrawText (str, tx - str_width / 2, metrics.height - metrics.y_origin + db_label_height);

		v_grid.MoveToPoint (tx, metrics.height - metrics.y_origin + 4);
		v_grid.AddLineToPoint (tx, metrics.y_origin);

		t += DCPTime::from_seconds (mark_interval);
	}

	gc->SetPen (wxPen (wxColour (200, 200, 200)));
	gc->StrokePath (v_grid);

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

	if (_cursor) {
		wxGraphicsPath cursor = gc->CreatePath ();
		cursor.MoveToPoint (_cursor->draw.x - _cursor_size / 2, _cursor->draw.y - _cursor_size / 2);
		cursor.AddLineToPoint (_cursor->draw.x + _cursor_size / 2, _cursor->draw.y + _cursor_size / 2);
		cursor.MoveToPoint (_cursor->draw.x + _cursor_size / 2, _cursor->draw.y - _cursor_size / 2);
		cursor.AddLineToPoint (_cursor->draw.x - _cursor_size / 2, _cursor->draw.y + _cursor_size / 2);
		gc->StrokePath (cursor);


	}

	delete gc;
}

float
AudioPlot::y_for_linear (float p, Metrics const & metrics) const
{
	if (p < 1e-4) {
		p = 1e-4;
	}

	return metrics.height - (20 * log10(p) - _minimum) * metrics.y_scale - metrics.y_origin;
}

void
AudioPlot::plot_peak (wxGraphicsPath& path, int channel, Metrics const & metrics) const
{
	if (_analysis->points (channel) == 0) {
		return;
	}

	_peak[channel] = PointList ();

	float peak = 0;
	int const N = _analysis->points(channel);
	for (int i = 0; i < N; ++i) {
		float const p = get_point(channel, i)[AudioPoint::PEAK];
		peak -= 0.01f * (1 - log10 (_smoothing) / log10 (max_smoothing));
		if (p > peak) {
			peak = p;
		} else if (peak < 0) {
			peak = 0;
		}

		_peak[channel].push_back (
			Point (
				wxPoint (metrics.db_label_width + i * metrics.x_scale, y_for_linear (peak, metrics)),
				DCPTime::from_frames (i * _analysis->samples_per_point(), _analysis->sample_rate()),
				20 * log10(peak)
				)
			);
	}

	DCPOMATIC_ASSERT (_peak.find(channel) != _peak.end());

	path.MoveToPoint (_peak[channel][0].draw);
	BOOST_FOREACH (Point const & i, _peak[channel]) {
		path.AddLineToPoint (i.draw);
	}
}

void
AudioPlot::plot_rms (wxGraphicsPath& path, int channel, Metrics const & metrics) const
{
	if (_analysis->points (channel) == 0) {
		return;
	}

	_rms[channel] = PointList();

	list<float> smoothing;

	int const N = _analysis->points(channel);

	float const first = get_point(channel, 0)[AudioPoint::RMS];
	float const last = get_point(channel, N - 1)[AudioPoint::RMS];

	int const before = _smoothing / 2;
	int const after = _smoothing - before;

	/* Pre-load the smoothing list */
	for (int i = 0; i < before; ++i) {
		smoothing.push_back (first);
	}
	for (int i = 0; i < after; ++i) {
		if (i < N) {
			smoothing.push_back (get_point(channel, i)[AudioPoint::RMS]);
		} else {
			smoothing.push_back (last);
		}
	}

	for (int i = 0; i < N; ++i) {

		int const next_for_window = i + after;

		if (next_for_window < N) {
			smoothing.push_back (get_point(channel, i)[AudioPoint::RMS]);
		} else {
			smoothing.push_back (last);
		}

		smoothing.pop_front ();

		float p = 0;
		for (list<float>::const_iterator j = smoothing.begin(); j != smoothing.end(); ++j) {
			p += pow (*j, 2);
		}

		if (!smoothing.empty ()) {
			p = sqrt (p / smoothing.size ());
		}

		_rms[channel].push_back (
			Point (
				wxPoint (metrics.db_label_width + i * metrics.x_scale, y_for_linear (p, metrics)),
				DCPTime::from_frames (i * _analysis->samples_per_point(), _analysis->sample_rate()),
				20 * log10(p)
				)
			);
	}

	DCPOMATIC_ASSERT (_rms.find(channel) != _rms.end());

	path.MoveToPoint (_rms[channel][0].draw);
	BOOST_FOREACH (Point const & i, _rms[channel]) {
		path.AddLineToPoint (i.draw);
	}
}

void
AudioPlot::set_smoothing (int s)
{
	_smoothing = s;
	_rms.clear ();
	_peak.clear ();
	Refresh ();
}

void
AudioPlot::set_gain_correction (double gain)
{
	_gain_correction = gain;
	Refresh ();
}

AudioPoint
AudioPlot::get_point (int channel, int point) const
{
	AudioPoint p = _analysis->get_point (channel, point);
	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		p[i] *= pow (10, _gain_correction / 20);
	}

	return p;
}

/** @param n Channel index.
 *  @return Colour used by that channel in the plot.
 */
wxColour
AudioPlot::colour (int n) const
{
	DCPOMATIC_ASSERT (n < int(_colours.size()));
	return _colours[n];
}

void
AudioPlot::search (map<int, PointList> const & search, wxMouseEvent const & ev, double& min_dist, Point& min_point) const
{
	for (map<int, PointList>::const_iterator i = search.begin(); i != search.end(); ++i) {
		BOOST_FOREACH (Point const & j, i->second) {
			double const dist = pow(ev.GetX() - j.draw.x, 2) + pow(ev.GetY() - j.draw.y, 2);
			if (dist < min_dist) {
				min_dist = dist;
				min_point = j;
			}
		}
	}
}

void
AudioPlot::mouse_moved (wxMouseEvent& ev)
{
	double min_dist = DBL_MAX;
	Point min_point;

	search (_rms, ev, min_dist, min_point);
	search (_peak, ev, min_dist, min_point);

	_cursor = optional<Point> ();

	if (min_dist < DBL_MAX) {
		wxRect before (min_point.draw.x - _cursor_size / 2, min_point.draw.y - _cursor_size / 2, _cursor_size, _cursor_size);
		GetParent()->Refresh (true, &before);
		_cursor = min_point;
		wxRect after (min_point.draw.x - _cursor_size / 2, min_point.draw.y - _cursor_size / 2, _cursor_size, _cursor_size);
		GetParent()->Refresh (true, &after);
		Cursor (min_point.time, min_point.db);
	}
}

void
AudioPlot::mouse_leave (wxMouseEvent &)
{
	_cursor = optional<Point> ();
	Refresh ();
	Cursor (optional<DCPTime>(), optional<float>());
}

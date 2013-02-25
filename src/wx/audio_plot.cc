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
#include "lib/decoder_factory.h"
#include "lib/audio_decoder.h"
#include "lib/audio_analysis.h"
#include "wx/wx_util.h"

using std::cout;
using std::vector;
using std::max;
using std::min;
using boost::bind;
using boost::shared_ptr;

int const AudioPlot::_minimum = -90;

AudioPlot::AudioPlot (wxWindow* parent)
	: wxPanel (parent)
	, _channel (0)
	, _gain (0)
{
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (AudioPlot::paint), 0, this);

	SetMinSize (wxSize (640, 512));
}

void
AudioPlot::set_analysis (shared_ptr<AudioAnalysis> a)
{
	_analysis = a;
	_channel = 0;
	Refresh ();
}

void
AudioPlot::set_channel (int c)
{
	_channel = c;
	Refresh ();
}

void
AudioPlot::paint (wxPaintEvent &)
{
	wxPaintDC dc (this);

	if (!_analysis) {
		return;
	}
	
	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int const width = GetSize().GetWidth();
	float const xs = width / float (_analysis->points (_channel));
	int const height = GetSize().GetHeight ();
	float const ys = height / -_minimum;

	wxGraphicsPath grid = gc->CreatePath ();
	gc->SetFont (gc->CreateFont (*wxSMALL_FONT));
	for (int i = _minimum; i <= 0; i += 10) {
		int const y = height - (i - _minimum) * ys;
		grid.MoveToPoint (0, y);
		grid.AddLineToPoint (width, y);
		gc->DrawText (std_to_wx (String::compose ("%1dB", i)), width - 32, y - 12);
	}
	gc->SetPen (*wxLIGHT_GREY_PEN);
	gc->StrokePath (grid);

	wxGraphicsPath path[AudioPoint::COUNT];

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		path[i] = gc->CreatePath ();
		path[i].MoveToPoint (0, height - (max (_analysis->get_point(_channel, 0)[i], float (_minimum)) - _minimum + _gain) * ys);
	}

	for (int i = 0; i < _analysis->points(_channel); ++i) {
		for (int j = 0; j < AudioPoint::COUNT; ++j) {
			path[j].AddLineToPoint (i * xs, height - (max (_analysis->get_point(_channel, i)[j], float (_minimum)) - _minimum + _gain) * ys);
		}
	}

	gc->SetPen (*wxBLUE_PEN);
	gc->StrokePath (path[AudioPoint::RMS]);

	gc->SetPen (*wxRED_PEN);
	gc->StrokePath (path[AudioPoint::PEAK]);

	delete gc;
}

void
AudioPlot::set_gain (float g)
{
	_gain = g;
	Refresh ();
}

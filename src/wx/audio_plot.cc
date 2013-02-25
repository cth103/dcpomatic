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
	, _gain (0)
{
	SetDoubleBuffered (true);

	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		_channel_visible[i] = false;
	}

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_visible[i] = false;
	}

	_colours.push_back (wxColour (  0,   0,   0));
	_colours.push_back (wxColour (255,   0,   0));
	_colours.push_back (wxColour (  0, 255,   0));
	_colours.push_back (wxColour (139,   0, 204));
	_colours.push_back (wxColour (  0,   0, 255));
	_colours.push_back (wxColour (100, 100, 100));
	
	Connect (wxID_ANY, wxEVT_PAINT, wxPaintEventHandler (AudioPlot::paint), 0, this);
	
	SetMinSize (wxSize (640, 512));
}

void
AudioPlot::set_analysis (shared_ptr<AudioAnalysis> a)
{
	_analysis = a;

	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		_channel_visible[i] = false;
	}

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_visible[i] = false;
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
AudioPlot::paint (wxPaintEvent &)
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	if (!_analysis) {
		gc->SetFont (gc->CreateFont (*wxNORMAL_FONT));
		gc->DrawText (_("Please wait; audio is being analysed..."), 32, 32);
		return;
	}

	wxGraphicsPath grid = gc->CreatePath ();
	gc->SetFont (gc->CreateFont (*wxSMALL_FONT));
	wxDouble db_label_width;
	wxDouble db_label_height;
	wxDouble db_label_descent;
	wxDouble db_label_leading;
	gc->GetTextExtent (_("-80dB"), &db_label_width, &db_label_height, &db_label_descent, &db_label_leading);

	db_label_width += 8;
	
	int const data_width = GetSize().GetWidth() - db_label_width;
	/* Assume all channels have the same number of points */
	float const xs = data_width / float (_analysis->points (0));
	int const height = GetSize().GetHeight ();
	int const yo = 32;
	float const ys = (height - yo) / -_minimum;

	for (int i = _minimum; i <= 0; i += 10) {
		int const y = (height - (i - _minimum) * ys) - yo;
		grid.MoveToPoint (db_label_width - 4, y);
		grid.AddLineToPoint (db_label_width + data_width, y);
		gc->DrawText (std_to_wx (String::compose ("%1dB", i)), 0, y - (db_label_height / 2));
	}
	gc->SetPen (*wxLIGHT_GREY_PEN);
	gc->StrokePath (grid);

	for (int c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
		if (!_channel_visible[c] || c >= _analysis->channels()) {
			continue;
		}

		wxGraphicsPath path[AudioPoint::COUNT];
		
		for (int i = 0; i < AudioPoint::COUNT; ++i) {
			if (!_type_visible[i]) {
				continue;
			}
			
			path[i] = gc->CreatePath ();
			path[i].MoveToPoint (
				db_label_width,
				height - (max (_analysis->get_point(c, 0)[i], float (_minimum)) - _minimum + _gain) * ys - yo
				);
		}

		for (int i = 0; i < _analysis->points(c); ++i) {
			for (int j = 0; j < AudioPoint::COUNT; ++j) {
				if (!_type_visible[j]) {
					continue;
				}
				
				path[j].AddLineToPoint (
					i * xs + db_label_width,
					height - (max (_analysis->get_point(c, i)[j], float (_minimum)) - _minimum + _gain) * ys - yo
					);
			}
		}

		wxColour const col = _colours[c];

		if (_type_visible[AudioPoint::RMS]) {
			gc->SetPen (*wxThePenList->FindOrCreatePen (col));
			gc->StrokePath (path[AudioPoint::RMS]);
		}

		if (_type_visible[AudioPoint::PEAK]) {
			gc->SetPen (*wxThePenList->FindOrCreatePen (wxColour (col.Red(), col.Green(), col.Blue(), col.Alpha() / 2)));
			gc->StrokePath (path[AudioPoint::PEAK]);
		}
	}

	wxGraphicsPath axes = gc->CreatePath ();
	axes.MoveToPoint (db_label_width, 0);
	axes.AddLineToPoint (db_label_width, height - yo);
	axes.AddLineToPoint (db_label_width + data_width, height - yo);
	gc->SetPen (*wxBLACK_PEN);
	gc->StrokePath (axes);

	delete gc;
}

void
AudioPlot::set_gain (float g)
{
	_gain = g;
	Refresh ();
}

/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <vector>
#include <boost/shared_ptr.hpp>
#include <wx/wx.h>
#include "lib/util.h"
#include "lib/audio_analysis.h"

class AudioPlot : public wxPanel
{
public:
	AudioPlot (wxWindow *);

	void set_analysis (boost::shared_ptr<AudioAnalysis>);
	void set_channel_visible (int c, bool v);
	void set_type_visible (int t, bool v);
	void set_gain (float);
	void set_smoothing (int);

	static const int max_smoothing;

private:
	void paint (wxPaintEvent &);

	boost::shared_ptr<AudioAnalysis> _analysis;
	bool _channel_visible[MAX_AUDIO_CHANNELS];
	bool _type_visible[AudioPoint::COUNT];
	/** gain to apply in dB */
	float _gain;
	int _smoothing;
	std::vector<wxColour> _colours;

	void plot_peak (wxGraphicsPath &, int) const;
	void plot_rms (wxGraphicsPath &, int) const;
	float y_for_linear (float) const;

	double _db_label_width;
	int _height;
	int _y_origin;
	float _x_scale;
	float _y_scale;

	static const int _minimum;
};

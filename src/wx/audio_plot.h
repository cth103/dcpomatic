/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <vector>
#include <boost/shared_ptr.hpp>
#include <wx/wx.h>
#include "lib/util.h"
#include "lib/audio_analysis.h"

struct Metrics;

class AudioPlot : public wxPanel
{
public:
	AudioPlot (wxWindow *);

	void set_analysis (boost::shared_ptr<AudioAnalysis>);
	void set_channel_visible (int c, bool v);
	void set_type_visible (int t, bool v);
	void set_smoothing (int);
	void set_message (wxString);
	void set_gain_correction (double gain);

	static const int max_smoothing;

private:
	void paint ();
	void plot_peak (wxGraphicsPath &, int, Metrics const &) const;
	void plot_rms (wxGraphicsPath &, int, Metrics const &) const;
	float y_for_linear (float, Metrics const &) const;
	AudioPoint get_point (int channel, int point) const;

	boost::shared_ptr<AudioAnalysis> _analysis;
	bool _channel_visible[MAX_DCP_AUDIO_CHANNELS];
	bool _type_visible[AudioPoint::COUNT];
	int _smoothing;
	std::vector<wxColour> _colours;
	wxString _message;
	float _gain_correction;

	static const int _minimum;
};

/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/util.h"
#include "lib/audio_analysis.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <vector>

struct Metrics;

class AudioPlot : public wxPanel
{
public:
	explicit AudioPlot (wxWindow *);

	void set_analysis (boost::shared_ptr<AudioAnalysis>);
	void set_channel_visible (int c, bool v);
	void set_type_visible (int t, bool v);
	void set_smoothing (int);
	void set_message (wxString);
	void set_gain_correction (double gain);

	wxColour colour (int n) const;

	boost::signals2::signal<void (boost::optional<DCPTime>, boost::optional<float>)> Cursor;

	static const int max_smoothing;

private:

	struct Point {
		Point ()
			: db(0)
		{}

		Point (wxPoint draw_, DCPTime time_, float db_)
			: draw(draw_)
			, time(time_)
			, db(db_)
		{}

		wxPoint draw;
		DCPTime time;
		float db;
	};

	typedef std::vector<Point> PointList;

	void paint ();
	void plot_peak (wxGraphicsPath &, int, Metrics const &) const;
	void plot_rms (wxGraphicsPath &, int, Metrics const &) const;
	float y_for_linear (float, Metrics const &) const;
	AudioPoint get_point (int channel, int point) const;
	void mouse_moved (wxMouseEvent& ev);
	void mouse_leave (wxMouseEvent& ev);
	void search (std::map<int, PointList> const & search, wxMouseEvent const & ev, double& min_dist, Point& min_point) const;

	boost::shared_ptr<AudioAnalysis> _analysis;
	bool _channel_visible[MAX_DCP_AUDIO_CHANNELS];
	bool _type_visible[AudioPoint::COUNT];
	int _smoothing;
	std::vector<wxColour> _colours;
	wxString _message;
	float _gain_correction;

	mutable std::map<int, PointList> _peak;
	mutable std::map<int, PointList> _rms;

	boost::optional<Point> _cursor;

	static const int _minimum;
	static const int _cursor_size;
};

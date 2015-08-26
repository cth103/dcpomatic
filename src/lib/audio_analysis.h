/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_ANALYSIS_H
#define DCPOMATIC_AUDIO_ANALYSIS_H

#include "dcpomatic_time.h"
#include "audio_point.h"
#include <libcxml/cxml.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <vector>

namespace xmlpp {
	class Element;
}

class AudioAnalysis : public boost::noncopyable
{
public:
	AudioAnalysis (int c);
	AudioAnalysis (boost::filesystem::path);

	void add_point (int c, AudioPoint const & p);
	void set_peak (float peak, DCPTime time) {
		_peak = peak;
		_peak_time = time;
	}

	AudioPoint get_point (int c, int p) const;
	int points (int c) const;
	int channels () const;

	boost::optional<float> peak () const {
		return _peak;
	}

	boost::optional<DCPTime> peak_time () const {
		return _peak_time;
	}

	boost::optional<double> analysis_gain () const {
		return _analysis_gain;
	}

	void set_analysis_gain (double gain) {
		_analysis_gain = gain;
	}

	void write (boost::filesystem::path);

private:
	std::vector<std::vector<AudioPoint> > _data;
	boost::optional<float> _peak;
	boost::optional<DCPTime> _peak_time;
	/** If this analysis was run on a single piece of
	 *  content we store its gain in dB when the analysis
	 *  happened.
	 */
	boost::optional<double> _analysis_gain;
};

#endif

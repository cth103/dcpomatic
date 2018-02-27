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

class Playlist;

class AudioAnalysis : public boost::noncopyable
{
public:
	explicit AudioAnalysis (int c);
	explicit AudioAnalysis (boost::filesystem::path);

	void add_point (int c, AudioPoint const & p);

	struct PeakTime {
		PeakTime (float p, DCPTime t)
			: peak (p)
			, time (t)
		{}

		float peak;
		DCPTime time;
	};

	void set_sample_peak (std::vector<PeakTime> peak) {
		_sample_peak = peak;
	}

	void set_true_peak (std::vector<float> peak) {
		_true_peak = peak;
	}

	void set_integrated_loudness (float l) {
		_integrated_loudness = l;
	}

	void set_loudness_range (float r) {
		_loudness_range = r;
	}

	AudioPoint get_point (int c, int p) const;
	int points (int c) const;
	int channels () const;

	std::vector<PeakTime> sample_peak () const {
		return _sample_peak;
	}

	std::pair<PeakTime, int> overall_sample_peak () const;

	std::vector<float> true_peak () const {
		return _true_peak;
	}

	boost::optional<float> overall_true_peak () const;

	boost::optional<float> integrated_loudness () const {
		return _integrated_loudness;
	}

	boost::optional<float> loudness_range () const {
		return _loudness_range;
	}

	boost::optional<double> analysis_gain () const {
		return _analysis_gain;
	}

	void set_analysis_gain (double gain) {
		_analysis_gain = gain;
	}

	void write (boost::filesystem::path);

	float gain_correction (boost::shared_ptr<const Playlist> playlist);

private:
	std::vector<std::vector<AudioPoint> > _data;
	std::vector<PeakTime> _sample_peak;
	std::vector<float> _true_peak;
	boost::optional<float> _integrated_loudness;
	boost::optional<float> _loudness_range;
	/** If this analysis was run on a single piece of
	 *  content we store its gain in dB when the analysis
	 *  happened.
	 */
	boost::optional<double> _analysis_gain;

	static int const _current_state_version;
};

#endif

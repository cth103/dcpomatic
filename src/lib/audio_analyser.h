/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_analysis.h"
#include "audio_filter_graph.h"
#include "dcpomatic_time.h"
#include "types.h"
#include <leqm_nrt.h>
#include <boost/scoped_ptr.hpp>
#include <memory>


class AudioAnalysis;
class AudioBuffers;
class AudioPoint;
class Film;
class Filter;
class Playlist;


class AudioAnalyser
{
public:
	AudioAnalyser (std::shared_ptr<const Film> film, std::shared_ptr<const Playlist> playlist, bool from_zero, std::function<void (float)> set_progress);

	AudioAnalyser (AudioAnalyser const&) = delete;
	AudioAnalyser& operator= (AudioAnalyser const&) = delete;

	void analyse (std::shared_ptr<AudioBuffers>, dcpomatic::DCPTime time);

	dcpomatic::DCPTime start () const {
		return _start;
	}

	void finish ();

	AudioAnalysis get () const {
		return _analysis;
	}

private:
	std::shared_ptr<const Film> _film;
	std::shared_ptr<const Playlist> _playlist;

	std::function<void (float)> _set_progress;

	dcpomatic::DCPTime _start;
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	AudioFilterGraph _ebur128;
#endif
	std::vector<Filter> _filters;
	Frame _samples_per_point = 1;

	boost::scoped_ptr<leqm_nrt::Calculator> _leqm;
	int _leqm_channels = 0;
	Frame _done = 0;
	std::vector<float> _sample_peak;
	std::vector<Frame> _sample_peak_frame;
	std::vector<AudioPoint> _current;

	AudioAnalysis _analysis;
};


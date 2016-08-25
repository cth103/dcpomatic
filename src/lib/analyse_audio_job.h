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

/** @file  src/lib/analyse_audio_job.h
 *  @brief AnalyseAudioJob class.
 */

#include "job.h"
#include "audio_point.h"
#include "types.h"

class AudioBuffers;
class AudioAnalysis;
class Playlist;
class AudioPoint;
class AudioFilterGraph;
class Filter;

/** @class AnalyseAudioJob
 *  @brief A job to analyse the audio of a film and make a note of its
 *  broad peak and RMS levels.
 *
 *  After computing the peak and RMS levels the job will write a file
 *  to Film::audio_analysis_path.
 */
class AnalyseAudioJob : public Job
{
public:
	AnalyseAudioJob (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);
	~AnalyseAudioJob ();

	std::string name () const;
	std::string json_name () const;
	void run ();

	boost::shared_ptr<const Playlist> playlist () const {
		return _playlist;
	}

private:
	void analyse (boost::shared_ptr<const AudioBuffers>);

	boost::shared_ptr<const Playlist> _playlist;

	int64_t _done;
	int64_t _samples_per_point;
	AudioPoint* _current;

	float* _sample_peak;
	Frame* _sample_peak_frame;

	boost::shared_ptr<AudioAnalysis> _analysis;

	boost::shared_ptr<AudioFilterGraph> _ebur128;
	std::vector<Filter const *> _filters;

	static const int _num_points;
};

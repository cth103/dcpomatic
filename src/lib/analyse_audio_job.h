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

/** @file  src/lib/analyse_audio_job.h
 *  @brief AnalyseAudioJob class.
 */

#include "job.h"
#include "types.h"

class AudioBuffers;
class AudioAnalysis;
class Playlist;

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

private:
	void analyse (boost::shared_ptr<const AudioBuffers>);

	boost::shared_ptr<const Playlist> _playlist;

	int64_t _done;
	int64_t _samples_per_point;
	AudioPoint* _current;

	float _overall_peak;
	Frame _overall_peak_frame;

	boost::shared_ptr<AudioAnalysis> _analysis;

	static const int _num_points;
};

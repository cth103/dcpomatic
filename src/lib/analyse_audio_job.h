/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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


#include "audio_analyser.h"
#include "audio_point.h"
#include "dcpomatic_time.h"
#include "job.h"
#include <leqm_nrt.h>
#include <boost/scoped_ptr.hpp>


class AudioAnalysis;
class AudioBuffers;
class AudioFilterGraph;
class AudioPoint;
class Filter;
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
	AnalyseAudioJob(std::shared_ptr<const Film>, std::shared_ptr<const Playlist>, bool whole_film);
	~AnalyseAudioJob ();

	std::string name () const override;
	std::string json_name () const override;
	void run () override;
	bool enable_notify () const override {
		return true;
	}

	boost::filesystem::path path () const {
		return _path;
	}

private:
	AudioAnalyser _analyser;

	std::shared_ptr<const Playlist> _playlist;
	/** playlist's audio analysis path when the job was created */
	boost::filesystem::path _path;
	bool _whole_film;

	static const int _num_points;
};

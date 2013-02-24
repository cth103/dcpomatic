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

#include "job.h"
#include "audio_analysis.h"

class AudioBuffers;

class AnalyseAudioJob : public Job
{
public:
	AnalyseAudioJob (boost::shared_ptr<Film> f);

	std::string name () const;
	void run ();

private:
	void audio (boost::shared_ptr<AudioBuffers>);

	int64_t _done_for_this_point;
	int64_t _done;
	int64_t _samples_per_point;
	std::vector<AudioPoint> _current;

	boost::shared_ptr<AudioAnalysis> _analysis;

	static const int _num_points;
};


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

#include "audio_analysis.h"
#include "analyse_audio_job.h"
#include "compose.hpp"
#include "film.h"
#include "options.h"
#include "playlist.h"

#include "i18n.h"

using std::string;
using std::max;
using std::min;
using std::cout;
using boost::shared_ptr;

int const AnalyseAudioJob::_num_points = 1024;

AnalyseAudioJob::AnalyseAudioJob (shared_ptr<Film> f)
	: Job (f)
	, _done (0)
	, _samples_per_point (1)
{

}

string
AnalyseAudioJob::name () const
{
	return String::compose (_("Analyse audio of %1"), _film->name());
}

void
AnalyseAudioJob::run ()
{
	shared_ptr<Playlist> playlist = _film->playlist ();
	playlist->disable_video ();
	
	playlist->Audio.connect (bind (&AnalyseAudioJob::audio, this, _1));

	_samples_per_point = max (int64_t (1), playlist->audio_length() / _num_points);

	_current.resize (playlist->audio_channels ());
	_analysis.reset (new AudioAnalysis (playlist->audio_channels()));
			 
	while (!playlist->pass()) {
		set_progress (float (_done) / playlist->audio_length ());
	}

	_analysis->write (_film->audio_analysis_path ());
	
	set_progress (1);
	set_state (FINISHED_OK);
}

void
AnalyseAudioJob::audio (shared_ptr<AudioBuffers> b)
{
	for (int i = 0; i < b->frames(); ++i) {
		for (int j = 0; j < b->channels(); ++j) {
			float s = b->data(j)[i];
			if (fabsf (s) < 10e-7) {
				/* stringstream can't serialise and recover inf or -inf, so prevent such
				   values by replacing with this (140dB down) */
				s = 10e-7;
			}
			_current[j][AudioPoint::RMS] += pow (s, 2);
			_current[j][AudioPoint::PEAK] = max (_current[j][AudioPoint::PEAK], fabsf (s));

			if ((_done % _samples_per_point) == 0) {
				_current[j][AudioPoint::RMS] = sqrt (_current[j][AudioPoint::RMS] / _samples_per_point);
				_analysis->add_point (j, _current[j]);
				
				_current[j] = AudioPoint ();
			}
		}

		++_done;
	}
}


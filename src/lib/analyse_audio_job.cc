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

#include "audio_analysis.h"
#include "audio_buffers.h"
#include "analyse_audio_job.h"
#include "audio_content.h"
#include "compose.hpp"
#include "film.h"
#include "player.h"
#include "playlist.h"
#include "filter.h"
#include "audio_filter_graph.h"
#include "config.h"
extern "C" {
#include <libavutil/channel_layout.h>
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
#include <libavfilter/f_ebur128.h>
#endif
}
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::max;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

int const AnalyseAudioJob::_num_points = 1024;

AnalyseAudioJob::AnalyseAudioJob (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
	: Job (film)
	, _playlist (playlist)
	, _done (0)
	, _samples_per_point (1)
	, _current (0)
	, _sample_peak (0)
	, _sample_peak_frame (0)
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	, _ebur128 (new AudioFilterGraph (film->audio_frame_rate(), film->audio_channels()))
#endif
{
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	_filters.push_back (new Filter ("ebur128", "ebur128", "audio", "ebur128=peak=true"));
	_ebur128->setup (_filters);
#endif
}

AnalyseAudioJob::~AnalyseAudioJob ()
{
	BOOST_FOREACH (Filter const * i, _filters) {
		delete const_cast<Filter*> (i);
	}
	delete[] _current;
}

string
AnalyseAudioJob::name () const
{
	return _("Analyse audio");
}

string
AnalyseAudioJob::json_name () const
{
	return N_("analyse_audio");
}

void
AnalyseAudioJob::run ()
{
	shared_ptr<Player> player (new Player (_film, _playlist));
	player->set_ignore_video ();
	player->set_fast ();
	player->set_play_referenced ();

	DCPTime const start = _playlist->start().get_value_or (DCPTime ());
	DCPTime const length = _playlist->length ();

	Frame const len = DCPTime (length - start).frames_round (_film->audio_frame_rate());
	_samples_per_point = max (int64_t (1), len / _num_points);

	delete[] _current;
	_current = new AudioPoint[_film->audio_channels ()];
	_analysis.reset (new AudioAnalysis (_film->audio_channels ()));

	bool has_any_audio = false;
	BOOST_FOREACH (shared_ptr<Content> c, _playlist->content ()) {
		if (c->audio) {
			has_any_audio = true;
		}
	}

	if (has_any_audio) {
		_done = 0;
		DCPTime const block = DCPTime::from_seconds (1.0 / 8);
		for (DCPTime t = start; t < length; t += block) {
			shared_ptr<const AudioBuffers> audio = player->get_audio (t, block, false);
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
			if (Config::instance()->analyse_ebur128 ()) {
				_ebur128->process (audio);
			}
#endif
			analyse (audio);
			set_progress ((t.seconds() - start.seconds()) / (length.seconds() - start.seconds()));
		}
	}

	_analysis->set_sample_peak (_sample_peak, DCPTime::from_frames (_sample_peak_frame, _film->audio_frame_rate ()));

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	if (Config::instance()->analyse_ebur128 ()) {
		void* eb = _ebur128->get("Parsed_ebur128_0")->priv;
		double true_peak = 0;
		for (int i = 0; i < _film->audio_channels(); ++i) {
			true_peak = max (true_peak, av_ebur128_get_true_peaks(eb)[i]);
		}
		_analysis->set_true_peak (true_peak);
		_analysis->set_integrated_loudness (av_ebur128_get_integrated_loudness(eb));
		_analysis->set_loudness_range (av_ebur128_get_loudness_range(eb));
	}
#endif

	if (_playlist->content().size() == 1) {
		/* If there was only one piece of content in this analysis we may later need to know what its
		   gain was when we analysed it.
		*/
		shared_ptr<const AudioContent> ac = _playlist->content().front()->audio;
		DCPOMATIC_ASSERT (ac);
		_analysis->set_analysis_gain (ac->gain ());
	}

	_analysis->write (_film->audio_analysis_path (_playlist));

	set_progress (1);
	set_state (FINISHED_OK);
}

void
AnalyseAudioJob::analyse (shared_ptr<const AudioBuffers> b)
{
	int const frames = b->frames ();
	int const channels = b->channels ();

	for (int j = 0; j < channels; ++j) {
		float* data = b->data(j);
		for (int i = 0; i < frames; ++i) {
			float s = data[i];
			float as = fabsf (s);
			if (as < 10e-7) {
				/* locked_stringstream can't serialise and recover inf or -inf, so prevent such
				   values by replacing with this (140dB down) */
				s = as = 10e-7;
			}
			_current[j][AudioPoint::RMS] += pow (s, 2);
			_current[j][AudioPoint::PEAK] = max (_current[j][AudioPoint::PEAK], as);

			if (as > _sample_peak) {
				_sample_peak = as;
				_sample_peak_frame = _done + i;
			}

			if (((_done + i) % _samples_per_point) == 0) {
				_current[j][AudioPoint::RMS] = sqrt (_current[j][AudioPoint::RMS] / _samples_per_point);
				_analysis->add_point (j, _current[j]);
				_current[j] = AudioPoint ();
			}
		}
	}

	_done += frames;
}

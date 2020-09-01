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

#include "audio_analysis.h"
#include "audio_buffers.h"
#include "analyse_audio_job.h"
#include "audio_content.h"
#include "compose.hpp"
#include "dcpomatic_log.h"
#include "film.h"
#include "player.h"
#include "playlist.h"
#include "filter.h"
#include "audio_filter_graph.h"
#include "config.h"
extern "C" {
#include <leqm_nrt.h>
#include <libavutil/channel_layout.h>
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
#include <libavfilter/f_ebur128.h>
#endif
}
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::vector;
using std::max;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

int const AnalyseAudioJob::_num_points = 1024;

static void add_if_required(vector<double>& v, size_t i, double db)
{
	if (v.size() > i) {
		v[i] = pow(10, db / 20);
	}
}

/** @param from_zero true to analyse audio from time 0 in the playlist, otherwise begin at Playlist::start */
AnalyseAudioJob::AnalyseAudioJob (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, bool from_zero)
	: Job (film)
	, _playlist (playlist)
	, _path (film->audio_analysis_path(playlist))
	, _from_zero (from_zero)
	, _done (0)
	, _samples_per_point (1)
	, _current (0)
	, _sample_peak (new float[film->audio_channels()])
	, _sample_peak_frame (new Frame[film->audio_channels()])
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	, _ebur128 (new AudioFilterGraph (film->audio_frame_rate(), film->audio_channels()))
#endif
{
	LOG_DEBUG_AUDIO_ANALYSIS_NC("AnalyseAudioJob::AnalyseAudioJob");

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	_filters.push_back (new Filter ("ebur128", "ebur128", "audio", "ebur128=peak=true"));
	_ebur128->setup (_filters);
#endif

	for (int i = 0; i < film->audio_channels(); ++i) {
		_sample_peak[i] = 0;
		_sample_peak_frame[i] = 0;
	}

	if (!_from_zero) {
		_start = _playlist->start().get_value_or(DCPTime());
	}

	/* XXX: is this right?  Especially for more than 5.1? */
	vector<double> channel_corrections(film->audio_channels(), 1);
	add_if_required (channel_corrections,  4,   -3); // Ls
	add_if_required (channel_corrections,  5,   -3); // Rs
	add_if_required (channel_corrections,  6, -144); // HI
	add_if_required (channel_corrections,  7, -144); // VI
	add_if_required (channel_corrections,  8,   -3); // Lc
	add_if_required (channel_corrections,  9,   -3); // Rc
	add_if_required (channel_corrections, 10,   -3); // Lc
	add_if_required (channel_corrections, 11,   -3); // Rc
	add_if_required (channel_corrections, 12, -144); // DBox
	add_if_required (channel_corrections, 13, -144); // Sync
	add_if_required (channel_corrections, 14, -144); // Sign Language
	add_if_required (channel_corrections, 15, -144); // Unused

	_leqm.reset(new leqm_nrt::Calculator(
		film->audio_channels(),
		film->audio_frame_rate(),
		24,
		channel_corrections,
		850, // suggested by leqm_nrt CLI source
		64,  // suggested by leqm_nrt CLI source
		boost::thread::hardware_concurrency()
		));
}

AnalyseAudioJob::~AnalyseAudioJob ()
{
	stop_thread ();
	BOOST_FOREACH (Filter const * i, _filters) {
		delete const_cast<Filter*> (i);
	}
	delete[] _current;
	delete[] _sample_peak;
	delete[] _sample_peak_frame;
}

string
AnalyseAudioJob::name () const
{
	return _("Analysing audio");
}

string
AnalyseAudioJob::json_name () const
{
	return N_("analyse_audio");
}

void
AnalyseAudioJob::run ()
{
	LOG_DEBUG_AUDIO_ANALYSIS_NC("AnalyseAudioJob::run");

	shared_ptr<Player> player (new Player(_film, _playlist));
	player->set_ignore_video ();
	player->set_ignore_text ();
	player->set_fast ();
	player->set_play_referenced ();
	player->Audio.connect (bind (&AnalyseAudioJob::analyse, this, _1, _2));

	DCPTime const length = _playlist->length (_film);

	Frame const len = DCPTime (length - _start).frames_round (_film->audio_frame_rate());
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
		LOG_DEBUG_AUDIO_ANALYSIS("Seeking to %1", to_string(_start));
		player->seek (_start, true);
		_done = 0;
		LOG_DEBUG_AUDIO_ANALYSIS("Starting loop for playlist of length %1", to_string(length));
		while (!player->pass ()) {}
	}

	LOG_DEBUG_AUDIO_ANALYSIS_NC("Loop complete");

	vector<AudioAnalysis::PeakTime> sample_peak;
	for (int i = 0; i < _film->audio_channels(); ++i) {
		sample_peak.push_back (
			AudioAnalysis::PeakTime (_sample_peak[i], DCPTime::from_frames (_sample_peak_frame[i], _film->audio_frame_rate ()))
			);
	}
	_analysis->set_sample_peak (sample_peak);

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	if (Config::instance()->analyse_ebur128 ()) {
		void* eb = _ebur128->get("Parsed_ebur128_0")->priv;
		vector<float> true_peak;
		for (int i = 0; i < _film->audio_channels(); ++i) {
			true_peak.push_back (av_ebur128_get_true_peaks(eb)[i]);
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
		if (ac) {
			_analysis->set_analysis_gain (ac->gain());
		}
	}

	_analysis->set_samples_per_point (_samples_per_point);
	_analysis->set_sample_rate (_film->audio_frame_rate ());
	_analysis->set_leqm (_leqm->leq_m());
	_analysis->write (_path);

	LOG_DEBUG_AUDIO_ANALYSIS_NC("Job finished");
	set_progress (1);
	set_state (FINISHED_OK);
}

void
AnalyseAudioJob::analyse (shared_ptr<const AudioBuffers> b, DCPTime time)
{
	LOG_DEBUG_AUDIO_ANALYSIS("Received %1 frames at %2", b->frames(), to_string(time));
	DCPOMATIC_ASSERT (time >= _start);

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	if (Config::instance()->analyse_ebur128 ()) {
		_ebur128->process (b);
	}
#endif

	int const frames = b->frames ();
	int const channels = b->channels ();
	vector<double> interleaved(frames * channels);

	for (int j = 0; j < channels; ++j) {
		float* data = b->data(j);
		for (int i = 0; i < frames; ++i) {
			float s = data[i];

			interleaved[i * channels + j] = s;

			float as = fabsf (s);
			if (as < 10e-7) {
				/* We may struggle to serialise and recover inf or -inf, so prevent such
				   values by replacing with this (140dB down) */
				s = as = 10e-7;
			}
			_current[j][AudioPoint::RMS] += pow (s, 2);
			_current[j][AudioPoint::PEAK] = max (_current[j][AudioPoint::PEAK], as);

			if (as > _sample_peak[j]) {
				_sample_peak[j] = as;
				_sample_peak_frame[j] = _done + i;
			}

			if (((_done + i) % _samples_per_point) == 0) {
				_current[j][AudioPoint::RMS] = sqrt (_current[j][AudioPoint::RMS] / _samples_per_point);
				_analysis->add_point (j, _current[j]);
				_current[j] = AudioPoint ();
			}
		}
	}

	_leqm->add(interleaved);

	_done += frames;

	DCPTime const length = _playlist->length (_film);
	set_progress ((time.seconds() - _start.seconds()) / (length.seconds() - _start.seconds()));
	LOG_DEBUG_AUDIO_ANALYSIS_NC("Frames processed");
}

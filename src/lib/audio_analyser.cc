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


#include "audio_analyser.h"
#include "audio_analysis.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_filter_graph.h"
#include "audio_point.h"
#include "config.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "filter.h"
#include "playlist.h"
#include <dcp/warnings.h>
extern "C" {
#include <leqm_nrt.h>
LIBDCP_DISABLE_WARNINGS
#include <libavutil/channel_layout.h>
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
#include <libavfilter/f_ebur128.h>
#endif
LIBDCP_ENABLE_WARNINGS
}


using std::make_shared;
using std::max;
using std::shared_ptr;
using std::vector;
using namespace dcpomatic;


static auto constexpr num_points = 1024;


AudioAnalyser::AudioAnalyser (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, bool from_zero, std::function<void (float)> set_progress)
	: _film (film)
	, _playlist (playlist)
	, _set_progress (set_progress)
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	, _ebur128(film->audio_frame_rate(), film->audio_channels())
#endif
	, _sample_peak (film->audio_channels())
	, _sample_peak_frame (film->audio_channels())
	, _analysis (film->audio_channels())
{

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	_filters.push_back({"ebur128", "ebur128", "audio", "ebur128=peak=true"});
	_ebur128.setup(_filters);
#endif

	_current = std::vector<AudioPoint>(_film->audio_channels());

	if (!from_zero) {
		_start = _playlist->start().get_value_or(DCPTime());
	}

	for (int i = 0; i < film->audio_channels(); ++i) {
		_sample_peak[i] = 0;
		_sample_peak_frame[i] = 0;
	}

	auto add_if_required = [](vector<double>& v, size_t i, double db) {
		if (v.size() > i) {
			v[i] = pow(10, db / 20);
		}
	};

	auto content = _playlist->content();
	if (content.size() == 1 && content[0]->audio) {
		_leqm_channels = 0;
		for (auto channel: content[0]->audio->mapping().mapped_output_channels()) {
			/* This means that if, for example, a file only maps C we will
			 * calculate LEQ(m) for L, R and C.  I'm not sure if this is
			 * right or not.
			 */
			_leqm_channels = std::min(film->audio_channels(), channel + 1);
		}
	} else {
		_leqm_channels = film->audio_channels();
	}

	/* XXX: is this right?  Especially for more than 5.1? */
	vector<double> channel_corrections(_leqm_channels, 1);
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
		_leqm_channels,
		film->audio_frame_rate(),
		24,
		channel_corrections,
		850, // suggested by leqm_nrt CLI source
		64,  // suggested by leqm_nrt CLI source
		boost::thread::hardware_concurrency()
		));

	DCPTime const length = _playlist->length (_film);

	Frame const len = DCPTime (length - _start).frames_round (film->audio_frame_rate());
	_samples_per_point = max (int64_t (1), len / num_points);
}


void
AudioAnalyser::analyse (shared_ptr<AudioBuffers> b, DCPTime time)
{
	LOG_DEBUG_AUDIO_ANALYSIS("AudioAnalyser received %1 frames at %2", b->frames(), to_string(time));
	DCPOMATIC_ASSERT (time >= _start);
	/* In bug #2364 we had a lot of frames arriving here (~47s worth) which
	 * caused an OOM error on Windows.  Check for the number of frames being
	 * reasonable here to make sure we catch this if it happens again.
	 */
	DCPOMATIC_ASSERT(b->frames() < 480000);

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	if (Config::instance()->analyse_ebur128 ()) {
		_ebur128.process(b);
	}
#endif

	int const frames = b->frames ();
	vector<double> interleaved(frames * _leqm_channels);

	for (int j = 0; j < _leqm_channels; ++j) {
		float const* data = b->data(j);
		for (int i = 0; i < frames; ++i) {
			float s = data[i];

			interleaved[i * _leqm_channels + j] = s;

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
				_analysis.add_point (j, _current[j]);
				_current[j] = AudioPoint ();
			}
		}
	}

	_leqm->add(interleaved);

	_done += frames;

	DCPTime const length = _playlist->length (_film);
	_set_progress ((time.seconds() - _start.seconds()) / (length.seconds() - _start.seconds()));
	LOG_DEBUG_AUDIO_ANALYSIS_NC("Frames processed");
}


void
AudioAnalyser::finish ()
{
	vector<AudioAnalysis::PeakTime> sample_peak;
	for (int i = 0; i < _film->audio_channels(); ++i) {
		sample_peak.push_back (
			AudioAnalysis::PeakTime (_sample_peak[i], DCPTime::from_frames (_sample_peak_frame[i], _film->audio_frame_rate ()))
			);
	}
	_analysis.set_sample_peak (sample_peak);

#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	if (Config::instance()->analyse_ebur128 ()) {
		void* eb = _ebur128.get("Parsed_ebur128_0")->priv;
		vector<float> true_peak;
		for (int i = 0; i < _film->audio_channels(); ++i) {
			true_peak.push_back (av_ebur128_get_true_peaks(eb)[i]);
		}
		_analysis.set_true_peak (true_peak);
		_analysis.set_integrated_loudness (av_ebur128_get_integrated_loudness(eb));
		_analysis.set_loudness_range (av_ebur128_get_loudness_range(eb));
	}
#endif

	if (_playlist->content().size() == 1) {
		/* If there was only one piece of content in this analysis we may later need to know what its
		   gain was when we analysed it.
		*/
		if (auto ac = _playlist->content().front()->audio) {
			_analysis.set_analysis_gain (ac->gain());
		}
	}

	_analysis.set_samples_per_point (_samples_per_point);
	_analysis.set_sample_rate (_film->audio_frame_rate ());
	_analysis.set_leqm (_leqm->leq_m());
}

/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_encoder.h"
#include "film.h"
#include "job.h"
#include "player.h"
#include "player_video.h"
#include "log.h"
#include "image.h"
#include "cross.h"
#include "butler.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::string;
using std::runtime_error;
using std::cout;
using std::pair;
using boost::shared_ptr;
using boost::bind;
using boost::weak_ptr;


FFmpegEncoder::FFmpegEncoder (shared_ptr<const Film> film, weak_ptr<Job> job, boost::filesystem::path output, ExportFormat format, bool mixdown_to_stereo, int x264_crf)
	: Encoder (film, job)
	, _file_encoder (
		_film->frame_size(),
		_film->video_frame_rate(),
		_film->audio_frame_rate(),
		mixdown_to_stereo ? 2 : film->audio_channels(),
		_film->log(),
		format,
		x264_crf,
		output
		)
	, _history (1000)
{
	_player->set_always_burn_open_subtitles ();
	_player->set_play_referenced ();

	int const ch = film->audio_channels ();

	AudioMapping map;
	if (mixdown_to_stereo) {
		_output_audio_channels = 2;
		map = AudioMapping (ch, 2);
		float const overall_gain = 2 / (4 + sqrt(2));
		float const minus_3dB = 1 / sqrt(2);
		map.set (dcp::LEFT,   0, overall_gain);
		map.set (dcp::RIGHT,  1, overall_gain);
		map.set (dcp::CENTRE, 0, overall_gain * minus_3dB);
		map.set (dcp::CENTRE, 1, overall_gain * minus_3dB);
		map.set (dcp::LS,     0, overall_gain);
		map.set (dcp::RS,     1, overall_gain);
	} else {
		_output_audio_channels = ch;
		map = AudioMapping (ch, ch);
		for (int i = 0; i < ch; ++i) {
			map.set (i, i, 1);
		}
	}

	_butler.reset (new Butler (_player, film->log(), map, _output_audio_channels));
}

void
FFmpegEncoder::go ()
{
	{
		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	DCPTime const video_frame = DCPTime::from_frames (1, _film->video_frame_rate ());
	int const audio_frames = video_frame.frames_round(_film->audio_frame_rate());
	float* interleaved = new float[_output_audio_channels * audio_frames];
	shared_ptr<AudioBuffers> deinterleaved (new AudioBuffers (_output_audio_channels, audio_frames));
	for (DCPTime i; i < _film->length(); i += video_frame) {
		pair<shared_ptr<PlayerVideo>, DCPTime> v = _butler->get_video ();
		_file_encoder.video (v.first, v.second);

		_history.event ();

		{
			boost::mutex::scoped_lock lm (_mutex);
			_last_time = i;
		}

		shared_ptr<Job> job = _job.lock ();
		if (job) {
			job->set_progress (float(i.get()) / _film->length().get());
		}

		_butler->get_audio (interleaved, audio_frames);
		/* XXX: inefficient; butler interleaves and we deinterleave again */
		float* p = interleaved;
		for (int j = 0; j < audio_frames; ++j) {
			for (int k = 0; k < _output_audio_channels; ++k) {
				deinterleaved->data(k)[j] = *p++;
			}
		}
		_file_encoder.audio (deinterleaved);
	}
	delete[] interleaved;

	_file_encoder.flush ();
}

float
FFmpegEncoder::current_rate () const
{
	return _history.rate ();
}

Frame
FFmpegEncoder::frames_done () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _last_time.frames_round (_film->video_frame_rate ());
}

/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_decoder.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "dcpomatic_log.h"
#include "log.h"
#include "resampler.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"


using std::cout;
using std::shared_ptr;
using std::make_shared;
using boost::optional;
using namespace dcpomatic;


AudioDecoder::AudioDecoder (Decoder* parent, shared_ptr<const AudioContent> content, bool fast)
	: DecoderPart (parent)
	, _content (content)
	, _fast (fast)
{
	/* Set up _positions so that we have one for each stream */
	for (auto i: content->streams ()) {
		_positions[i] = 0;
	}
}


/** @param time_already_delayed true if the delay should not be added to time */
void
AudioDecoder::emit(shared_ptr<const Film> film, AudioStreamPtr stream, shared_ptr<const AudioBuffers> data, ContentTime time, bool flushing)
{
	if (ignore ()) {
		return;
	}

	int const resampled_rate = _content->resampled_frame_rate(film);
	if (!flushing) {
		time += ContentTime::from_seconds (_content->delay() / 1000.0);
	}

	/* Amount of error we will tolerate on audio timestamps; see comment below.
	 * We'll use 1 24fps video frame as this seems to be roughly how ffplay does it.
	 */
	Frame const slack_frames = resampled_rate / 24;

	/* first_since_seek is set to true if this is the first data we have
	   received since initialisation or seek.  We'll set the position based
	   on the ContentTime that was given.  After this first time we just
	   count samples unless the timestamp is more than slack_frames away
	   from where we think it should be.  This is because ContentTimes seem
	   to be slightly unreliable from FFmpegDecoder (i.e.  not sample
	   accurate), but we still need to obey them sometimes otherwise we get
	   sync problems such as #1833.
	*/

	auto const first_since_seek = _positions[stream] == 0;
	auto const need_reset = !first_since_seek && (std::abs(_positions[stream] - time.frames_round(resampled_rate)) > slack_frames);

	if (need_reset) {
		LOG_GENERAL (
			"Reset audio position: was %1, new data at %2, slack: %3 frames",
			_positions[stream],
			time.frames_round(resampled_rate),
			std::abs(_positions[stream] - time.frames_round(resampled_rate))
			);
	}

	if (first_since_seek || need_reset) {
		_positions[stream] = time.frames_round (resampled_rate);
	}

	if (first_since_seek && _content->delay() > 0) {
		silence (stream, _content->delay());
	}

	shared_ptr<Resampler> resampler;
	auto i = _resamplers.find(stream);
	if (i != _resamplers.end()) {
		resampler = i->second;
	} else {
		if (stream->frame_rate() != resampled_rate) {
			LOG_GENERAL (
				"Creating new resampler from %1 to %2 with %3 channels",
				stream->frame_rate(),
				resampled_rate,
				stream->channels()
				);

			resampler = make_shared<Resampler>(stream->frame_rate(), resampled_rate, stream->channels());
			if (_fast) {
				resampler->set_fast ();
			}
			_resamplers[stream] = resampler;
		}
	}

	if (resampler && !flushing) {
		auto ro = resampler->run (data);
		if (ro->frames() == 0) {
			return;
		}
		data = ro;
	}

	Data(stream, ContentAudio (data, _positions[stream]));
	_positions[stream] += data->frames();
}


/** @return Time just after the last thing that was emitted from a given stream */
ContentTime
AudioDecoder::stream_position (shared_ptr<const Film> film, AudioStreamPtr stream) const
{
	auto i = _positions.find (stream);
	DCPOMATIC_ASSERT (i != _positions.end ());
	return ContentTime::from_frames (i->second, _content->resampled_frame_rate(film));
}


boost::optional<ContentTime>
AudioDecoder::position (shared_ptr<const Film> film) const
{
	optional<ContentTime> p;
	for (auto i: _positions) {
		auto const ct = stream_position (film, i.first);
		if (!p || ct < *p) {
			p = ct;
		}
	}

	return p;
}


void
AudioDecoder::seek ()
{
	for (auto i: _resamplers) {
		i.second->flush ();
		i.second->reset ();
	}

	for (auto& i: _positions) {
		i.second = 0;
	}
}


void
AudioDecoder::flush ()
{
	for (auto const& i: _resamplers) {
		auto ro = i.second->flush ();
		if (ro->frames() > 0) {
			Data (i.first, ContentAudio (ro, _positions[i.first]));
			_positions[i.first] += ro->frames();
		}
	}

	if (_content->delay() < 0) {
		/* Finish off with the gap caused by the delay */
		for (auto stream: _content->streams()) {
			silence (stream, -_content->delay());
		}
	}
}


void
AudioDecoder::silence (AudioStreamPtr stream, int milliseconds)
{
	int const samples = ContentTime::from_seconds(milliseconds / 1000.0).frames_round(stream->frame_rate());
	auto silence = make_shared<AudioBuffers>(stream->channels(), samples);
	silence->make_silent ();
	Data (stream, ContentAudio(silence, _positions[stream]));
}

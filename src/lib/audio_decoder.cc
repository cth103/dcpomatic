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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "log.h"
#include "resampler.h"
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL(...) _log->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);

using std::cout;
using std::map;
using std::pair;
using boost::shared_ptr;
using boost::optional;

AudioDecoder::AudioDecoder (Decoder* parent, shared_ptr<const AudioContent> content, shared_ptr<Log> log, bool fast)
	: DecoderPart (parent, log)
	, _content (content)
	, _fast (fast)
{
	/* Set up _positions so that we have one for each stream */
	BOOST_FOREACH (AudioStreamPtr i, content->streams ()) {
		_positions[i] = 0;
	}
}

void
AudioDecoder::emit (AudioStreamPtr stream, shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (ignore ()) {
		return;
	}

	if (_positions[stream] == 0) {
		/* This is the first data we have received since initialisation or seek.  Set
		   the position based on the ContentTime that was given.  After this first time
		   we just count samples, as it seems that ContentTimes are unreliable from
		   FFmpegDecoder (not quite continuous; perhaps due to some rounding error).
		*/
		if (_content->delay() > 0) {
			/* Insert silence to give the delay */
			silence (_content->delay ());
		}
		time += ContentTime::from_seconds (_content->delay() / 1000.0);
		_positions[stream] = time.frames_round (_content->resampled_frame_rate ());
	}

	shared_ptr<Resampler> resampler;
	map<AudioStreamPtr, shared_ptr<Resampler> >::iterator i = _resamplers.find(stream);
	if (i != _resamplers.end ()) {
		resampler = i->second;
	} else {
		if (stream->frame_rate() != _content->resampled_frame_rate()) {
			LOG_GENERAL (
				"Creating new resampler from %1 to %2 with %3 channels",
				stream->frame_rate(),
				_content->resampled_frame_rate(),
				stream->channels()
				);

			resampler.reset (new Resampler (stream->frame_rate(), _content->resampled_frame_rate(), stream->channels()));
			if (_fast) {
				resampler->set_fast ();
			}
			_resamplers[stream] = resampler;
		}
	}

	if (resampler) {
		shared_ptr<const AudioBuffers> ro = resampler->run (data);
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
AudioDecoder::stream_position (AudioStreamPtr stream) const
{
	map<AudioStreamPtr, Frame>::const_iterator i = _positions.find (stream);
	DCPOMATIC_ASSERT (i != _positions.end ());
	return ContentTime::from_frames (i->second, _content->resampled_frame_rate());
}

ContentTime
AudioDecoder::position () const
{
	optional<ContentTime> p;
	for (map<AudioStreamPtr, Frame>::const_iterator i = _positions.begin(); i != _positions.end(); ++i) {
		ContentTime const ct = stream_position (i->first);
		if (!p || ct < *p) {
			p = ct;
		}
	}

	return p.get_value_or(ContentTime());
}

void
AudioDecoder::seek ()
{
	for (map<AudioStreamPtr, shared_ptr<Resampler> >::iterator i = _resamplers.begin(); i != _resamplers.end(); ++i) {
		i->second->flush ();
		i->second->reset ();
	}

	for (map<AudioStreamPtr, Frame>::iterator i = _positions.begin(); i != _positions.end(); ++i) {
		i->second = 0;
	}
}

void
AudioDecoder::flush ()
{
	for (map<AudioStreamPtr, shared_ptr<Resampler> >::iterator i = _resamplers.begin(); i != _resamplers.end(); ++i) {
		shared_ptr<const AudioBuffers> ro = i->second->flush ();
		if (ro->frames() > 0) {
			Data (i->first, ContentAudio (ro, _positions[i->first]));
			_positions[i->first] += ro->frames();
		}
	}

	if (_content->delay() < 0) {
		/* Finish off with the gap caused by the delay */
		silence (-_content->delay ());
	}
}

void
AudioDecoder::silence (int milliseconds)
{
	BOOST_FOREACH (AudioStreamPtr i, _content->streams ()) {
		int const samples = ContentTime::from_seconds(milliseconds / 1000.0).frames_round(i->frame_rate());
		shared_ptr<AudioBuffers> silence (new AudioBuffers (i->channels(), samples));
		silence->make_silent ();
		Data (i, ContentAudio (silence, _positions[i]));
	}
}

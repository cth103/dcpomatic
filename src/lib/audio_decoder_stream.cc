/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "audio_decoder_stream.h"
#include "audio_buffers.h"
#include "audio_processor.h"
#include "audio_decoder.h"
#include "resampler.h"
#include "util.h"
#include "film.h"
#include "log.h"
#include "audio_content.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::list;
using std::pair;
using std::cout;
using std::min;
using std::max;
using boost::optional;
using boost::shared_ptr;

AudioDecoderStream::AudioDecoderStream (
	shared_ptr<const AudioContent> content, AudioStreamPtr stream, Decoder* decoder, AudioDecoder* audio_decoder, shared_ptr<Log> log
	)
	: _content (content)
	, _stream (stream)
	, _decoder (decoder)
	, _audio_decoder (audio_decoder)
	, _log (log)
	  /* We effectively start having done a seek to zero; this allows silence-padding of the first
	     data that comes out of our decoder.
	  */
	, _seek_reference (ContentTime ())
{
	if (content->resampled_frame_rate() != _stream->frame_rate() && _stream->channels() > 0) {
		_resampler.reset (new Resampler (_stream->frame_rate(), content->resampled_frame_rate(), _stream->channels ()));
	}

	reset_decoded ();
}

void
AudioDecoderStream::reset_decoded ()
{
	_decoded = ContentAudio (shared_ptr<AudioBuffers> (new AudioBuffers (_stream->channels(), 0)), 0);
}

/** Audio timestamping is made hard by many factors, but perhaps the most entertaining is resampling.
 *  We have to assume that we are feeding continuous data into the resampler, and so we get continuous
 *  data out.  Hence we do the timestamping here, post-resampler, just by counting samples.
 *
 *  The time is passed in here so that after a seek we can set up our _position.  The
 *  time is ignored once this has been done.
 */
void
AudioDecoderStream::audio (shared_ptr<const AudioBuffers> data, ContentTime time)
{
	_log->log (String::compose ("ADS receives %1 %2", to_string(time), data->frames ()), LogEntry::TYPE_DEBUG_DECODE);

	if (_resampler) {
		data = _resampler->run (data);
	}

	Frame const frame_rate = _content->resampled_frame_rate ();

	if (_seek_reference) {
		/* We've had an accurate seek and now we're seeing some data */
		ContentTime const delta = time - _seek_reference.get ();
		Frame const delta_frames = delta.frames_round (frame_rate);
		if (delta_frames > 0) {
			/* This data comes after the seek time.  Pad the data with some silence. */
			shared_ptr<AudioBuffers> padded (new AudioBuffers (data->channels(), data->frames() + delta_frames));
			padded->make_silent ();
			padded->copy_from (data.get(), data->frames(), 0, delta_frames);
			data = padded;
			time -= delta;
		}
		_seek_reference = optional<ContentTime> ();
	}

	if (!_position) {
		_position = time.frames_round (frame_rate);
	}

	DCPOMATIC_ASSERT (_position.get() >= (_decoded.frame + _decoded.audio->frames()));

	add (data);
}

void
AudioDecoderStream::add (shared_ptr<const AudioBuffers> data)
{
	if (!_position) {
		/* This should only happen when there is a seek followed by a flush, but
		   we need to cope with it.
		*/
		return;
	}

	/* Resize _decoded to fit the new data */
	int new_size = 0;
	if (_decoded.audio->frames() == 0) {
		/* There's nothing in there, so just store the new data */
		new_size = data->frames ();
		_decoded.frame = _position.get ();
	} else {
		/* Otherwise we need to extend _decoded to include the new stuff */
		new_size = _position.get() + data->frames() - _decoded.frame;
	}

	_decoded.audio->ensure_size (new_size);
	_decoded.audio->set_frames (new_size);

	/* Copy new data in */
	_decoded.audio->copy_from (data.get(), data->frames(), 0, _position.get() - _decoded.frame);
	_position = _position.get() + data->frames ();

	/* Limit the amount of data we keep in case nobody is asking for it */
	int const max_frames = _content->resampled_frame_rate () * 10;
	if (_decoded.audio->frames() > max_frames) {
		int const to_remove = _decoded.audio->frames() - max_frames;
		_decoded.frame += to_remove;
		_decoded.audio->move (to_remove, 0, max_frames);
		_decoded.audio->set_frames (max_frames);
	}
}

void
AudioDecoderStream::flush ()
{
	if (!_resampler) {
		return;
	}

	shared_ptr<const AudioBuffers> b = _resampler->flush ();
	if (b) {
		add (b);
	}
}

void
AudioDecoderStream::set_fast ()
{
	if (_resampler) {
		_resampler->set_fast ();
	}
}

optional<ContentTime>
AudioDecoderStream::position () const
{
	if (!_position) {
		return optional<ContentTime> ();
	}

	return ContentTime::from_frames (_position.get(), _content->resampled_frame_rate());
}

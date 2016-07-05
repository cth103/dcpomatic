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

AudioDecoderStream::AudioDecoderStream (shared_ptr<const AudioContent> content, AudioStreamPtr stream, Decoder* decoder, shared_ptr<Log> log)
	: _content (content)
	, _stream (stream)
	, _decoder (decoder)
	, _log (log)
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

ContentAudio
AudioDecoderStream::get (Frame frame, Frame length, bool accurate)
{
	shared_ptr<ContentAudio> dec;

	_log->log (String::compose ("-> ADS has request for %1 %2", frame, length), LogEntry::TYPE_DEBUG_DECODE);

	Frame const end = frame + length - 1;

	/* If we are less than (about) 5 seconds behind the data that we want we'll
	   run through it rather than seeking.
	*/
	Frame const slack = 5 * 48000;

	if (frame < _decoded.frame || end > (_decoded.frame + _decoded.audio->frames() + slack)) {
		/* Either we have no decoded data, all our data is after the time that we
		   want, or what we do have is a long way from what we want: seek */
		_decoder->seek (ContentTime::from_frames (frame, _content->resampled_frame_rate()), accurate);
	}

	/* Offset of the data that we want from the start of _decoded.audio
	   (to be set up shortly)
	*/
	Frame decoded_offset = 0;

	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 *
	 * If we are being accurate, we want the right frames,
	 * otherwise any frames will do.
	 */
	if (accurate) {
		/* Keep stuffing data into _decoded until we have enough data, or the subclass does not want to give us any more */
		while (
			(_decoded.frame > frame || (_decoded.frame + _decoded.audio->frames()) < end) &&
			!_decoder->pass (Decoder::PASS_REASON_AUDIO, accurate)
			)
		{}

		decoded_offset = frame - _decoded.frame;

		_log->log (
			String::compose ("Accurate ADS::get has offset %1 from request %2 and available %3", decoded_offset, frame, _decoded.frame),
			LogEntry::TYPE_DEBUG_DECODE
			);
	} else {
		while (
			_decoded.audio->frames() < length &&
			!_decoder->pass (Decoder::PASS_REASON_AUDIO, accurate)
			)
		{}

		/* Use decoded_offset of 0, as we don't really care what frames we return */
	}

	/* The amount of data available in _decoded.audio starting from `frame'.  This could be -ve
	   if pass() returned true before we got enough data.
	*/
	Frame const available = _decoded.audio->frames() - decoded_offset;

	/* We will return either that, or the requested amount, whichever is smaller */
	Frame const to_return = max ((Frame) 0, min (available, length));

	/* Copy our data to the output */
	shared_ptr<AudioBuffers> out (new AudioBuffers (_decoded.audio->channels(), to_return));
	out->copy_from (_decoded.audio.get(), to_return, decoded_offset, 0);

	Frame const remaining = max ((Frame) 0, available - to_return);

	/* Clean up decoded; first, move the data after what we just returned to the start of the buffer */
	_decoded.audio->move (decoded_offset + to_return, 0, remaining);
	/* And set up the number of frames we have left */
	_decoded.audio->set_frames (remaining);
	/* Also bump where those frames are in terms of the content */
	_decoded.frame += decoded_offset + to_return;

	return ContentAudio (out, frame);
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
	_log->log (String::compose ("ADS receives %1 %2", time, data->frames ()), LogEntry::TYPE_DEBUG_DECODE);

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
AudioDecoderStream::seek (ContentTime t, bool accurate)
{
	_position.reset ();
	reset_decoded ();
	if (accurate) {
		_seek_reference = t;
	}
}

void
AudioDecoderStream::set_fast ()
{
	if (_resampler) {
		_resampler->set_fast ();
	}
}

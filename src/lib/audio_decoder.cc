/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "audio_processor.h"
#include "resampler.h"
#include "util.h"
#include <iostream>

#include "i18n.h"

using std::list;
using std::pair;
using std::cout;
using std::min;
using std::max;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const AudioContent> content)
	: _audio_content (content)
{
	if (content->resampled_audio_frame_rate() != content->audio_frame_rate() && content->audio_channels ()) {
		_resampler.reset (new Resampler (content->audio_frame_rate(), content->resampled_audio_frame_rate(), content->audio_channels ()));
	}

	if (content->audio_processor ()) {
		_processor = content->audio_processor()->clone (content->resampled_audio_frame_rate ());
	}

	reset_decoded_audio ();
}

void
AudioDecoder::reset_decoded_audio ()
{
	_decoded_audio = ContentAudio (shared_ptr<AudioBuffers> (new AudioBuffers (_audio_content->processed_audio_channels(), 0)), 0);
}

shared_ptr<ContentAudio>
AudioDecoder::get_audio (AudioFrame frame, AudioFrame length, bool accurate)
{
	shared_ptr<ContentAudio> dec;

	AudioFrame const end = frame + length - 1;
		
	if (frame < _decoded_audio.frame || end > (_decoded_audio.frame + length * 4)) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (ContentTime::from_frames (frame, _audio_content->audio_frame_rate()), accurate);
	}

	/* Offset of the data that we want from the start of _decoded_audio.audio
	   (to be set up shortly)
	*/
	AudioFrame decoded_offset = 0;
	
	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 *
	 * If we are being accurate, we want the right frames,
	 * otherwise any frames will do.
	 */
	if (accurate) {
		/* Keep stuffing data into _decoded_audio until we have enough data, or the subclass does not want to give us any more */
		while ((_decoded_audio.frame > frame || (_decoded_audio.frame + _decoded_audio.audio->frames()) < end) && !pass ()) {}
		decoded_offset = frame - _decoded_audio.frame;
	} else {
		while (_decoded_audio.audio->frames() < length && !pass ()) {}
		/* Use decoded_offset of 0, as we don't really care what frames we return */
	}

	/* The amount of data available in _decoded_audio.audio starting from `frame'.  This could be -ve
	   if pass() returned true before we got enough data.
	*/
	AudioFrame const available = _decoded_audio.audio->frames() - decoded_offset;

	/* We will return either that, or the requested amount, whichever is smaller */
	AudioFrame const to_return = max ((AudioFrame) 0, min (available, length));

	/* Copy our data to the output */
	shared_ptr<AudioBuffers> out (new AudioBuffers (_decoded_audio.audio->channels(), to_return));
	out->copy_from (_decoded_audio.audio.get(), to_return, decoded_offset, 0);

	AudioFrame const remaining = max ((AudioFrame) 0, available - to_return);

	/* Clean up decoded; first, move the data after what we just returned to the start of the buffer */
	_decoded_audio.audio->move (decoded_offset + to_return, 0, remaining);
	/* And set up the number of frames we have left */
	_decoded_audio.audio->set_frames (remaining);
	/* Also bump where those frames are in terms of the content */
	_decoded_audio.frame += decoded_offset + to_return;

	return shared_ptr<ContentAudio> (new ContentAudio (out, frame));
}

/** Called by subclasses when audio data is ready.
 *
 *  Audio timestamping is made hard by many factors, but perhaps the most entertaining is resampling.
 *  We have to assume that we are feeding continuous data into the resampler, and so we get continuous
 *  data out.  Hence we do the timestamping here, post-resampler, just by counting samples.
 *
 *  The time is passed in here so that after a seek we can set up our _audio_position.  The
 *  time is ignored once this has been done.
 */
void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (_resampler) {
		data = _resampler->run (data);
	}

	if (_processor) {
		data = _processor->run (data);
	}

	AudioFrame const frame_rate = _audio_content->resampled_audio_frame_rate ();

	if (_seek_reference) {
		/* We've had an accurate seek and now we're seeing some data */
		ContentTime const delta = time - _seek_reference.get ();
		AudioFrame const delta_frames = delta.frames (frame_rate);
		if (delta_frames > 0) {
			/* This data comes after the seek time.  Pad the data with some silence. */
			shared_ptr<AudioBuffers> padded (new AudioBuffers (data->channels(), data->frames() + delta_frames));
			padded->make_silent ();
			padded->copy_from (data.get(), data->frames(), 0, delta_frames);
			data = padded;
			time -= delta;
		} else if (delta_frames < 0) {
			/* This data comes before the seek time.  Throw some data away */
			AudioFrame const to_discard = min (-delta_frames, static_cast<AudioFrame> (data->frames()));
			AudioFrame const to_keep = data->frames() - to_discard;
			if (to_keep == 0) {
				/* We have to throw all this data away, so keep _seek_reference and
				   try again next time some data arrives.
				*/
				return;
			}
			shared_ptr<AudioBuffers> trimmed (new AudioBuffers (data->channels(), to_keep));
			trimmed->copy_from (data.get(), to_keep, to_discard, 0);
			data = trimmed;
			time += ContentTime::from_frames (to_discard, frame_rate);
		}
		_seek_reference = optional<ContentTime> ();
	}

	if (!_audio_position) {
		_audio_position = time.frames (frame_rate);
	}

	assert (_audio_position.get() >= (_decoded_audio.frame + _decoded_audio.audio->frames()));

	add (data);
}

void
AudioDecoder::add (shared_ptr<const AudioBuffers> data)
{
	if (!_audio_position) {
		/* This should only happen when there is a seek followed by a flush, but
		   we need to cope with it.
		*/
		return;
	}
	
	/* Resize _decoded_audio to fit the new data */
	int new_size = 0;
	if (_decoded_audio.audio->frames() == 0) {
		/* There's nothing in there, so just store the new data */
		new_size = data->frames ();
		_decoded_audio.frame = _audio_position.get ();
	} else {
		/* Otherwise we need to extend _decoded_audio to include the new stuff */
		new_size = _audio_position.get() + data->frames() - _decoded_audio.frame;
	}
	
	_decoded_audio.audio->ensure_size (new_size);
	_decoded_audio.audio->set_frames (new_size);

	/* Copy new data in */
	_decoded_audio.audio->copy_from (data.get(), data->frames(), 0, _audio_position.get() - _decoded_audio.frame);
	_audio_position = _audio_position.get() + data->frames ();

	/* Limit the amount of data we keep in case nobody is asking for it */
	int const max_frames = _audio_content->resampled_audio_frame_rate () * 10;
	if (_decoded_audio.audio->frames() > max_frames) {
		int const to_remove = _decoded_audio.audio->frames() - max_frames;
		_decoded_audio.frame += to_remove;
		_decoded_audio.audio->move (to_remove, 0, max_frames);
		_decoded_audio.audio->set_frames (max_frames);
	}
}

void
AudioDecoder::flush ()
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
AudioDecoder::seek (ContentTime t, bool accurate)
{
	_audio_position.reset ();
	reset_decoded_audio ();
	if (accurate) {
		_seek_reference = t;
	}
	if (_processor) {
		_processor->flush ();
	}
}

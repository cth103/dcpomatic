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
#include "exceptions.h"
#include "log.h"
#include "resampler.h"
#include "util.h"
#include "film.h"

#include "i18n.h"

using std::stringstream;
using std::list;
using std::pair;
using std::cout;
using std::min;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const AudioContent> content)
	: _audio_content (content)
	, _decoded_audio (shared_ptr<AudioBuffers> (new AudioBuffers (content->audio_channels(), 0)), 0)
{
	if (content->output_audio_frame_rate() != content->content_audio_frame_rate() && content->audio_channels ()) {
		_resampler.reset (new Resampler (content->content_audio_frame_rate(), content->output_audio_frame_rate(), content->audio_channels ()));
	}
}

shared_ptr<ContentAudio>
AudioDecoder::get_audio (AudioFrame frame, AudioFrame length, bool accurate)
{
	shared_ptr<ContentAudio> dec;

	AudioFrame const end = frame + length - 1;
		
	if (frame < _decoded_audio.frame || end > (_decoded_audio.frame + length * 4)) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (ContentTime::from_frames (frame, _audio_content->content_audio_frame_rate()), accurate);
	}

	AudioFrame decoded_offset = 0;
	
	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 *
	 * If we are being accurate, we want the right frames,
	 * otherwise any frames will do.
	 */
	if (accurate) {
		while (!pass() && _decoded_audio.audio->frames() < length) {}
		/* Use decoded_offset of 0, as we don't really care what frames we return */
	} else {
		while (!pass() && (_decoded_audio.frame > frame || (_decoded_audio.frame + _decoded_audio.audio->frames()) < end)) {}
		decoded_offset = frame - _decoded_audio.frame;
	}

	AudioFrame const amount_left = _decoded_audio.audio->frames() - decoded_offset;
	
	AudioFrame const to_return = min (amount_left, length);
	shared_ptr<AudioBuffers> out (new AudioBuffers (_decoded_audio.audio->channels(), to_return));
	out->copy_from (_decoded_audio.audio.get(), to_return, decoded_offset, 0);
	
	/* Clean up decoded */
	_decoded_audio.audio->move (decoded_offset + to_return, 0, amount_left - to_return);
	_decoded_audio.audio->set_frames (amount_left - to_return);

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

	if (!_audio_position) {
		_audio_position = time.frames (_audio_content->output_audio_frame_rate ());
	}

	assert (_audio_position >= (_decoded_audio.frame + _decoded_audio.audio->frames()));

	/* Resize _decoded_audio to fit the new data */
	int const new_size = _audio_position.get() + data->frames() - _decoded_audio.frame;
	_decoded_audio.audio->ensure_size (new_size);
	_decoded_audio.audio->set_frames (new_size);

	/* Copy new data in */
	_decoded_audio.audio->copy_from (data.get(), data->frames(), 0, _audio_position.get() - _decoded_audio.frame);
	_audio_position = _audio_position.get() + data->frames ();
}

/* XXX: called? */
void
AudioDecoder::flush ()
{
	if (!_resampler) {
		return;
	}

	/*
	shared_ptr<const AudioBuffers> b = _resampler->flush ();
	if (b) {
		_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (b, _audio_position.get ())));
		_audio_position = _audio_position.get() + b->frames ();
	}
	*/
}

void
AudioDecoder::seek (ContentTime, bool)
{
	_audio_position.reset ();
}

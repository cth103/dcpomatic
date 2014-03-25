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

/** @file src/lib/audio_decoder.h
 *  @brief Parent class for audio decoders.
 */

#ifndef DCPOMATIC_AUDIO_DECODER_H
#define DCPOMATIC_AUDIO_DECODER_H

#include "decoder.h"
#include "content.h"
#include "audio_content.h"
#include "content_audio.h"

class AudioBuffers;
class Resampler;

/** @class AudioDecoder.
 *  @brief Parent class for audio decoders.
 */
class AudioDecoder : public virtual Decoder
{
public:
	AudioDecoder (boost::shared_ptr<const AudioContent>);
	
	boost::shared_ptr<const AudioContent> audio_content () const {
		return _audio_content;
	}

	/** Try to fetch some audio from a specific place in this content.
	 *  @param frame Frame to start from.
	 *  @param length Frames to get.
	 *  @param accurate true to try hard to return frames from exactly `frame', false if we don't mind nearby frames.
	 *  @return Time-stamped audio data which may or may not be from the location (and of the length) requested.
	 */
	boost::shared_ptr<ContentAudio> get_audio (AudioFrame time, AudioFrame length, bool accurate);
	
protected:

	void seek (ContentTime time, bool accurate);
	void audio (boost::shared_ptr<const AudioBuffers>, ContentTime);
	void flush ();

	boost::shared_ptr<const AudioContent> _audio_content;
	boost::shared_ptr<Resampler> _resampler;
	boost::optional<AudioFrame> _audio_position;
	/** Currently-available decoded audio data */
	ContentAudio _decoded_audio;
};

#endif

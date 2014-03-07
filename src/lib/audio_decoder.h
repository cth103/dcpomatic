/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
#include "decoded.h"

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

	void seek (ContentTime time, bool accurate);
	
protected:

	void audio (boost::shared_ptr<const AudioBuffers>, ContentTime);
	void flush ();

	boost::shared_ptr<const AudioContent> _audio_content;
	boost::shared_ptr<Resampler> _resampler;
	boost::optional<ContentTime> _audio_position;
};

#endif

/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_DECODER_STREAM_H
#define DCPOMATIC_AUDIO_DECODER_STREAM_H

#include "audio_stream.h"
#include "content_audio.h"
#include <boost/shared_ptr.hpp>

class AudioContent;
class AudioDecoder;
class Resampler;

class AudioDecoderStream
{
public:
	AudioDecoderStream (boost::shared_ptr<const AudioContent>, AudioStreamPtr, AudioDecoder* decoder);

	ContentAudio get (Frame time, Frame length, bool accurate);
	void audio (boost::shared_ptr<const AudioBuffers>, ContentTime);
	void flush ();
	void seek (ContentTime time, bool accurate);

private:

	void reset_decoded ();
	void add (boost::shared_ptr<const AudioBuffers>);

	boost::shared_ptr<const AudioContent> _content;
	AudioStreamPtr _stream;
	AudioDecoder* _decoder;
	boost::shared_ptr<Resampler> _resampler;
	boost::optional<Frame> _position;
	/** Currently-available decoded audio data */
	ContentAudio _decoded;
	/** The time of an accurate seek after which we have not yet received any actual
	    data at the seek time.
	*/
	boost::optional<ContentTime> _seek_reference;
};

#endif

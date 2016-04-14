/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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
#include "audio_decoder_stream.h"
#include "audio_content.h"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::map;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const AudioContent> content, bool fast, shared_ptr<Log> log)
	: _audio_content (content)
	, _ignore_audio (false)
	, _fast (fast)
{
	BOOST_FOREACH (AudioStreamPtr i, content->streams ()) {
		_streams[i] = shared_ptr<AudioDecoderStream> (new AudioDecoderStream (_audio_content, i, this, log));
	}
}

ContentAudio
AudioDecoder::get_audio (AudioStreamPtr stream, Frame frame, Frame length, bool accurate)
{
	return _streams[stream]->get (frame, length, accurate);
}

void
AudioDecoder::audio (AudioStreamPtr stream, shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (_ignore_audio) {
		return;
	}

	if (_streams.find (stream) == _streams.end ()) {

		/* This method can be called with an unknown stream during the following sequence:
		   - Add KDM to some DCP content.
		   - Content gets re-examined.
		   - SingleStreamAudioContent::take_from_audio_examiner creates a new stream.
		   - Some content property change signal is delivered so Player::Changed is emitted.
		   - Film viewer to re-gets the frame.
		   - Player calls DCPDecoder pass which calls this method on the new stream.

		   At this point the AudioDecoder does not know about the new stream.

		   Then
		   - Some other property change signal is delivered which marks the player's pieces invalid.
		   - Film viewer re-gets again.
		   - Everything is OK.

		   In this situation it is fine for us to silently drop the audio.
		*/

		return;
	}

	_streams[stream]->audio (data, time);
}

void
AudioDecoder::flush ()
{
	for (map<AudioStreamPtr, shared_ptr<AudioDecoderStream> >::const_iterator i = _streams.begin(); i != _streams.end(); ++i) {
		i->second->flush ();
	}
}

void
AudioDecoder::seek (ContentTime t, bool accurate)
{
	for (map<AudioStreamPtr, shared_ptr<AudioDecoderStream> >::const_iterator i = _streams.begin(); i != _streams.end(); ++i) {
		i->second->seek (t, accurate);
	}
}

/** Set this player never to produce any audio data */
void
AudioDecoder::set_ignore_audio ()
{
	_ignore_audio = true;
}

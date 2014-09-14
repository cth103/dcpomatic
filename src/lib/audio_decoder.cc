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

#include <iostream>
#include "audio_decoder.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "log.h"
#include "resampler.h"

#include "i18n.h"

using std::list;
using std::pair;
using std::cout;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const Film> film, shared_ptr<const AudioContent> content)
	: Decoder (film)
	, _audio_content (content)
	, _audio_position (0)
{

}

void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, AudioContent::Frame frame)
{
	Audio (data, frame);
	_audio_position = frame + data->frames ();
}

/** This is a bit odd, but necessary when we have (e.g.) FFmpegDecoders with no audio.
 *  The player needs to know that there is no audio otherwise it will keep trying to
 *  pass() the decoder to get it to emit audio.
 */
bool
AudioDecoder::has_audio () const
{
	return _audio_content->audio_channels () > 0;
}

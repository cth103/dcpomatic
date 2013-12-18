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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "log.h"
#include "resampler.h"

#include "i18n.h"

using std::stringstream;
using std::list;
using std::pair;
using std::cout;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const Film> film, shared_ptr<const AudioContent> content)
	: Decoder (film)
	, _audio_content (content)
	, _last_audio (0)
{
	if (content->output_audio_frame_rate() != content->content_audio_frame_rate() && content->audio_channels ()) {
		_resampler.reset (new Resampler (content->content_audio_frame_rate(), content->output_audio_frame_rate(), content->audio_channels ()));
	}
}

void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (_resampler) {
		data = _resampler->run (data);
	}
	
	_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (data, time)));
	_last_audio = time + (data->frames() * TIME_HZ / _audio_content->output_audio_frame_rate());
}

void
AudioDecoder::flush ()
{
	if (!_resampler) {
		return;
	}

	shared_ptr<const AudioBuffers> b = _resampler->flush ();
	if (b) {
		audio (b, _last_audio);
	}
}

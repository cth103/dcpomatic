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
#include "util.h"
#include "film.h"

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
{
	if (content->output_audio_frame_rate() != content->content_audio_frame_rate() && content->audio_channels ()) {
		_resampler.reset (new Resampler (content->content_audio_frame_rate(), content->output_audio_frame_rate(), content->audio_channels ()));
	}
}

/** Audio timestamping is made hard by many factors, but the final nail in the coffin is resampling.
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
		shared_ptr<const Film> film = _film.lock ();
		assert (film);
		_audio_position = time;
	}

	_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (_audio_position.get (), data)));
	_audio_position = _audio_position.get() + ContentTime (data->frames (), _audio_content->output_audio_frame_rate ());
}

void
AudioDecoder::flush ()
{
	if (!_resampler) {
		return;
	}

	shared_ptr<const AudioBuffers> b = _resampler->flush ();
	if (b) {
		_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (_audio_position.get (), b)));
		_audio_position = _audio_position.get() + ContentTime (b->frames (), _audio_content->output_audio_frame_rate ());
	}
}

void
AudioDecoder::seek (ContentTime, bool)
{
	_audio_position.reset ();
}

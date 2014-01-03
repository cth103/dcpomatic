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
	, _audio_position (0)
{
	if (content->output_audio_frame_rate() != content->content_audio_frame_rate() && content->audio_channels ()) {
		_resampler.reset (new Resampler (content->content_audio_frame_rate(), content->output_audio_frame_rate(), content->audio_channels ()));
	}
}

/** Audio timestamping is made hard by many factors, but the final nail in the coffin is resampling.
 *  We have to assume that we are feeding continuous data into the resampler, and so we get continuous
 *  data out.  Hence we do the timestamping here, post-resampler, just by counting samples.
 */
void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data)
{
	if (_resampler) {
		data = _resampler->run (data);
	}

	_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (data, _audio_position)));
	_audio_position += data->frames ();
}

void
AudioDecoder::flush ()
{
	if (!_resampler) {
		return;
	}

	shared_ptr<const AudioBuffers> b = _resampler->flush ();
	if (b) {
		_pending.push_back (shared_ptr<DecodedAudio> (new DecodedAudio (b, _audio_position)));
		_audio_position += b->frames ();
	}
}

void
AudioDecoder::seek (ContentTime t, bool)
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	FrameRateChange frc = film->active_frame_rate_change (_audio_content->position ());
	_audio_position = ((t + first_audio()) / frc.speed_up) * film->audio_frame_rate() / TIME_HZ;
}

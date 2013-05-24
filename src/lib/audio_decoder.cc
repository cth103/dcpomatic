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

#include "i18n.h"

using std::stringstream;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const Film> f, shared_ptr<const AudioContent> c)
	: Decoder (f)
	, _next_audio (0)
	, _audio_content (c)
{
	if (_audio_content->content_audio_frame_rate() != _audio_content->output_audio_frame_rate()) {

		shared_ptr<const Film> film = _film.lock ();
		assert (film);

		stringstream s;
		s << String::compose (
			"Will resample audio from %1 to %2",
			_audio_content->content_audio_frame_rate(), _audio_content->output_audio_frame_rate()
			);
		
		film->log()->log (s.str ());

		/* We will be using planar float data when we call the
		   resampler.  As far as I can see, the audio channel
		   layout is not necessary for our purposes; it seems
		   only to be used get the number of channels and
		   decide if rematrixing is needed.  It won't be, since
		   input and output layouts are the same.
		*/

		_swr_context = swr_alloc_set_opts (
			0,
			av_get_default_channel_layout (MAX_AUDIO_CHANNELS),
			AV_SAMPLE_FMT_FLTP,
			_audio_content->output_audio_frame_rate(),
			av_get_default_channel_layout (MAX_AUDIO_CHANNELS),
			AV_SAMPLE_FMT_FLTP,
			_audio_content->content_audio_frame_rate(),
			0, 0
			);
		
		swr_init (_swr_context);
	} else {
		_swr_context = 0;
	}
}

AudioDecoder::~AudioDecoder ()
{
	if (_swr_context) {
		swr_free (&_swr_context);
	}
}
	

#if 0
void
AudioDecoder::process_end ()
{
	if (_swr_context) {

		shared_ptr<const Film> film = _film.lock ();
		assert (film);
		
		shared_ptr<AudioBuffers> out (new AudioBuffers (film->audio_mapping().dcp_channels(), 256));
			
		while (1) {
			int const frames = swr_convert (_swr_context, (uint8_t **) out->data(), 256, 0, 0);

			if (frames < 0) {
				throw EncodeError (_("could not run sample-rate converter"));
			}

			if (frames == 0) {
				break;
			}

			out->set_frames (frames);
			_writer->write (out);
		}

	}
}
#endif

void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, Time time)
{
	/* Maybe resample */
	if (_swr_context) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil (
			(int64_t) data->frames() * _audio_content->output_audio_frame_rate() / _audio_content->content_audio_frame_rate()
			) + 32;

		shared_ptr<AudioBuffers> resampled (new AudioBuffers (data->channels(), max_resampled_frames));

		/* Resample audio */
		int const resampled_frames = swr_convert (
			_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) data->data(), data->frames()
			);
		
		if (resampled_frames < 0) {
			throw EncodeError (_("could not run sample-rate converter"));
		}

		resampled->set_frames (resampled_frames);
		
		/* And point our variables at the resampled audio */
		data = resampled;
	}

	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	/* Remap channels */
	shared_ptr<AudioBuffers> dcp_mapped (film->dcp_audio_channels(), data->frames());
	dcp_mapped->make_silent ();
	list<pair<int, libdcp::Channel> > map = _audio_content->audio_mapping().content_to_dcp ();
	for (list<pair<int, libdcp::Channel> >::iterator i = map.begin(); i != map.end(); ++i) {
		dcp_mapped->accumulate (data, i->first, i->second);
	}

	Audio (dcp_mapped, time);
	_next_audio = time + film->audio_frames_to_time (data->frames());
}

		

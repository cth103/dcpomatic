/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
}	
#include "resampler.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "compose.hpp"

#include "i18n.h"

using std::cout;
using std::pair;
using std::make_pair;
using boost::shared_ptr;

Resampler::Resampler (int in, int out, int channels)
	: _in_rate (in)
	, _out_rate (out)
	, _channels (channels)
{
	_swr_context = swr_alloc ();

	/* Sample formats */
	av_opt_set_int (_swr_context, "isf", AV_SAMPLE_FMT_FLTP, 0);
	av_opt_set_int (_swr_context, "osf", AV_SAMPLE_FMT_FLTP, 0);

	/* Channel counts */
	av_opt_set_int (_swr_context, "ich", _channels, 0);
	av_opt_set_int (_swr_context, "och", _channels, 0);

	/* Sample rates */
	av_opt_set_int (_swr_context, "isr", _in_rate, 0);
	av_opt_set_int (_swr_context, "osr", _out_rate, 0);
	
	swr_init (_swr_context);
}

Resampler::~Resampler ()
{
	swr_free (&_swr_context);
}

pair<shared_ptr<const AudioBuffers>, AudioContent::Frame>
Resampler::run (shared_ptr<const AudioBuffers> in, AudioContent::Frame frame)
{
	AudioContent::Frame const resamp_time = swr_next_pts (_swr_context, frame * _out_rate) / _in_rate;
		
	/* Compute the resampled frames count and add 32 for luck */
	int const max_resampled_frames = ceil ((double) in->frames() * _out_rate / _in_rate) + 32;
	shared_ptr<AudioBuffers> resampled (new AudioBuffers (_channels, max_resampled_frames));

	int const resampled_frames = swr_convert (
		_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) in->data(), in->frames()
		);
	
	if (resampled_frames < 0) {
		char buf[256];
		av_strerror (resampled_frames, buf, sizeof(buf));
		throw EncodeError (String::compose (_("could not run sample-rate converter for %1 samples (%2) (%3)"), in->frames(), resampled_frames, buf));
	}
	
	resampled->set_frames (resampled_frames);
	return make_pair (resampled, resamp_time);
}	

shared_ptr<const AudioBuffers>
Resampler::flush ()
{
	shared_ptr<AudioBuffers> out (new AudioBuffers (_channels, 0));
	int out_offset = 0;
	int64_t const pass_size = 256;
	shared_ptr<AudioBuffers> pass (new AudioBuffers (_channels, 256));

	while (true) {
		int const frames = swr_convert (_swr_context, (uint8_t **) pass->data(), pass_size, 0, 0);
		
		if (frames < 0) {
			throw EncodeError (_("could not run sample-rate converter"));
		}
		
		if (frames == 0) {
			break;
		}

		out->ensure_size (out_offset + frames);
		out->copy_from (pass.get(), frames, 0, out_offset);
		out_offset += frames;
		out->set_frames (out_offset);
	}

	return out;
}

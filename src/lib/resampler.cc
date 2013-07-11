extern "C" {
#include "libavutil/channel_layout.h"
}	
#include "resampler.h"
#include "audio_buffers.h"
#include "exceptions.h"

#include "i18n.h"

using boost::shared_ptr;

Resampler::Resampler (int in, int out, int channels)
	: _in_rate (in)
	, _out_rate (out)
	, _channels (channels)
{
	/* We will be using planar float data when we call the
	   resampler.  As far as I can see, the audio channel
	   layout is not necessary for our purposes; it seems
	   only to be used get the number of channels and
	   decide if rematrixing is needed.  It won't be, since
	   input and output layouts are the same.
	*/

	_swr_context = swr_alloc_set_opts (
		0,
		av_get_default_channel_layout (_channels),
		AV_SAMPLE_FMT_FLTP,
		_out_rate,
		av_get_default_channel_layout (_channels),
		AV_SAMPLE_FMT_FLTP,
		_in_rate,
		0, 0
		);
	
	swr_init (_swr_context);
}

Resampler::~Resampler ()
{
	swr_free (&_swr_context);
}

shared_ptr<const AudioBuffers>
Resampler::run (shared_ptr<const AudioBuffers> in)
{
	/* Compute the resampled frames count and add 32 for luck */
	int const max_resampled_frames = ceil ((double) in->frames() * _out_rate / _in_rate) + 32;
	shared_ptr<AudioBuffers> resampled (new AudioBuffers (_channels, max_resampled_frames));

	int const resampled_frames = swr_convert (
		_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) in->data(), in->frames()
		);
	
	if (resampled_frames < 0) {
		throw EncodeError (_("could not run sample-rate converter"));
	}
	
	resampled->set_frames (resampled_frames);
	return resampled;
}	

shared_ptr<const AudioBuffers>
Resampler::flush ()
{
	shared_ptr<AudioBuffers> out (new AudioBuffers (_channels, 0));
	int out_offset = 0;
	int64_t const pass_size = 256;
	shared_ptr<AudioBuffers> pass (new AudioBuffers (_channels, 256));

	while (1) {
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

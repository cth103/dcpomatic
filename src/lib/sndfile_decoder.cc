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

#include <iostream>
#include <sndfile.h>
#include "audio_content.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "exceptions.h"
#include "audio_buffers.h"

#include "i18n.h"

using std::vector;
using std::string;
using std::min;
using std::cout;
using boost::shared_ptr;

SndfileDecoder::SndfileDecoder (shared_ptr<const SndfileContent> c, bool fast, shared_ptr<Log> log)
	: Sndfile (c)
	, AudioDecoder (c->audio, fast, log)
	, _done (0)
	, _remaining (_info.frames)
	, _deinterleave_buffer (0)
{

}

SndfileDecoder::~SndfileDecoder ()
{
	delete[] _deinterleave_buffer;
}

bool
SndfileDecoder::pass (PassReason, bool)
{
	if (_remaining == 0) {
		return true;
	}

	/* Do things in half second blocks as I think there may be limits
	   to what FFmpeg (and in particular the resampler) can cope with.
	*/
	sf_count_t const block = _sndfile_content->audio->stream()->frame_rate() / 2;
	sf_count_t const this_time = min (block, _remaining);

	int const channels = _sndfile_content->audio->stream()->channels ();

	shared_ptr<AudioBuffers> data (new AudioBuffers (channels, this_time));

	if (_sndfile_content->audio->stream()->channels() == 1) {
		/* No de-interleaving required */
		sf_read_float (_sndfile, data->data(0), this_time);
	} else {
		/* Deinterleave */
		if (!_deinterleave_buffer) {
			_deinterleave_buffer = new float[block * channels];
		}
		sf_readf_float (_sndfile, _deinterleave_buffer, this_time);
		vector<float*> out_ptr (channels);
		for (int i = 0; i < channels; ++i) {
			out_ptr[i] = data->data(i);
		}
		float* in_ptr = _deinterleave_buffer;
		for (int i = 0; i < this_time; ++i) {
			for (int j = 0; j < channels; ++j) {
				*out_ptr[j]++ = *in_ptr++;
			}
		}
	}

	data->set_frames (this_time);
	audio (_sndfile_content->audio->stream (), data, ContentTime::from_frames (_done, _info.samplerate));
	_done += this_time;
	_remaining -= this_time;

	return _remaining == 0;
}

void
SndfileDecoder::seek (ContentTime t, bool accurate)
{
	AudioDecoder::seek (t, accurate);

	_done = t.frames_round (_info.samplerate);
	_remaining = _info.frames - _done;
}

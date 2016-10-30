/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "resampler.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "compose.hpp"
#include "dcpomatic_assert.h"
#include <samplerate.h>
#include <iostream>
#include <cmath>

#include "i18n.h"

using std::cout;
using std::pair;
using std::make_pair;
using std::runtime_error;
using boost::shared_ptr;

/** @param in Input sampling rate (Hz)
 *  @param out Output sampling rate (Hz)
 *  @param channels Number of channels.
 */
Resampler::Resampler (int in, int out, int channels)
	: _in_rate (in)
	, _out_rate (out)
	, _channels (channels)
{
	int error;
	_src = src_new (SRC_SINC_BEST_QUALITY, _channels, &error);
	if (!_src) {
		throw runtime_error (String::compose (N_("could not create sample-rate converter (%1)"), error));
	}
}

Resampler::~Resampler ()
{
	src_delete (_src);
}

void
Resampler::set_fast ()
{
	src_delete (_src);
	int error;
	_src = src_new (SRC_LINEAR, _channels, &error);
	if (!_src) {
		throw runtime_error (String::compose (N_("could not create sample-rate converter (%1)"), error));
	}
}

shared_ptr<const AudioBuffers>
Resampler::run (shared_ptr<const AudioBuffers> in)
{
	int in_frames = in->frames ();
	int in_offset = 0;
	int out_offset = 0;
	shared_ptr<AudioBuffers> resampled (new AudioBuffers (_channels, 0));

	while (in_frames > 0) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil ((double) in_frames * _out_rate / _in_rate) + 32;

		SRC_DATA data;
		float* in_buffer = new float[in_frames * _channels];

		{
			float** p = in->data ();
			float* q = in_buffer;
			for (int i = 0; i < in_frames; ++i) {
				for (int j = 0; j < _channels; ++j) {
					*q++ = p[j][in_offset + i];
				}
			}
		}

		data.data_in = in_buffer;
		data.input_frames = in_frames;

		data.data_out = new float[max_resampled_frames * _channels];
		data.output_frames = max_resampled_frames;

		data.end_of_input = 0;
		data.src_ratio = double (_out_rate) / _in_rate;

		int const r = src_process (_src, &data);
		if (r) {
			delete[] data.data_in;
			delete[] data.data_out;
			throw EncodeError (
				String::compose (
					N_("could not run sample-rate converter (%1) [processing %2 to %3, %4 channels]"),
					src_strerror (r),
					in_frames,
					max_resampled_frames,
					_channels
					)
				);
		}

		if (data.output_frames_gen == 0) {
			break;
		}

		resampled->ensure_size (out_offset + data.output_frames_gen);
		resampled->set_frames (out_offset + data.output_frames_gen);

		{
			float* p = data.data_out;
			float** q = resampled->data ();
			for (int i = 0; i < data.output_frames_gen; ++i) {
				for (int j = 0; j < _channels; ++j) {
					q[j][out_offset + i] = *p++;
				}
			}
		}

		in_frames -= data.input_frames_used;
		in_offset += data.input_frames_used;
		out_offset += data.output_frames_gen;

		delete[] data.data_in;
		delete[] data.data_out;
	}

	return resampled;
}

shared_ptr<const AudioBuffers>
Resampler::flush ()
{
	shared_ptr<AudioBuffers> out (new AudioBuffers (_channels, 0));
	int out_offset = 0;
	int64_t const output_size = 65536;

	float dummy[1];
	float* buffer = new float[output_size];

	SRC_DATA data;
	data.data_in = dummy;
	data.input_frames = 0;
	data.data_out = buffer;
	data.output_frames = output_size;
	data.end_of_input = 1;
	data.src_ratio = double (_out_rate) / _in_rate;

	int const r = src_process (_src, &data);
	if (r) {
		delete[] buffer;
		throw EncodeError (String::compose (N_("could not run sample-rate converter (%1)"), src_strerror (r)));
	}

	out->ensure_size (out_offset + data.output_frames_gen);

	float* p = data.data_out;
	float** q = out->data ();
	for (int i = 0; i < data.output_frames_gen; ++i) {
		for (int j = 0; j < _channels; ++j) {
			q[j][out_offset + i] = *p++;
		}
	}

	out_offset += data.output_frames_gen;
	out->set_frames (out_offset);

	delete[] buffer;
	return out;
}

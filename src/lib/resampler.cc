/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_buffers.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "resampler.h"
#include <samplerate.h>
#include <iostream>
#include <cmath>

#include "i18n.h"


using std::cout;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::runtime_error;
using std::shared_ptr;


/** @param in Input sampling rate (Hz)
 *  @param out Output sampling rate (Hz)
 *  @param channels Number of channels.
 */
Resampler::Resampler(int in, int out, int channels)
	: _in_rate(in)
	, _out_rate(out)
	, _channels(channels)
{
	int error;
	_src = src_new(SRC_SINC_BEST_QUALITY, _channels, &error);
	if (!_src) {
		throw runtime_error(fmt::format(N_("could not create sample-rate converter ({})"), error));
	}
}


Resampler::~Resampler()
{
	if (_src) {
		src_delete(_src);
	}
}


void
Resampler::set_fast()
{
	src_delete(_src);
	_src = nullptr;

	int error;
	_src = src_new(SRC_LINEAR, _channels, &error);
	if (!_src) {
		throw runtime_error(fmt::format(N_("could not create sample-rate converter ({})"), error));
	}
}


shared_ptr<const AudioBuffers>
Resampler::run(shared_ptr<const AudioBuffers> in)
{
	DCPOMATIC_ASSERT(in->channels() == _channels);

	int in_frames = in->frames();
	int in_offset = 0;
	int out_offset = 0;
	auto resampled = make_shared<AudioBuffers>(_channels, 0);

	while (in_frames > 0) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil(static_cast<double>(in_frames) * _out_rate / _in_rate) + 32;

		SRC_DATA data;
		std::vector<float> in_buffer(in_frames * _channels);
		std::vector<float> out_buffer(max_resampled_frames * _channels);

		{
			auto p = in->data();
			auto q = in_buffer.data();
			for (int i = 0; i < in_frames; ++i) {
				for (int j = 0; j < _channels; ++j) {
					*q++ = p[j][in_offset + i];
				}
			}
		}

		data.data_in = in_buffer.data();
		data.input_frames = in_frames;

		data.data_out = out_buffer.data();
		data.output_frames = max_resampled_frames;

		data.end_of_input = 0;
		data.src_ratio = double(_out_rate) / _in_rate;

		int const r = src_process(_src, &data);
		if (r) {
			throw EncodeError(
				fmt::format(
					N_("could not run sample-rate converter ({}) [processing {} to {}, {} channels]"),
					src_strerror(r),
					in_frames,
					max_resampled_frames,
					_channels
					)
				);
		}

		if (data.output_frames_gen == 0) {
			break;
		}

		resampled->set_frames(out_offset + data.output_frames_gen);

		{
			auto p = data.data_out;
			auto q = resampled->data();
			for (int i = 0; i < data.output_frames_gen; ++i) {
				for (int j = 0; j < _channels; ++j) {
					q[j][out_offset + i] = *p++;
				}
			}
		}

		in_frames -= data.input_frames_used;
		in_offset += data.input_frames_used;
		out_offset += data.output_frames_gen;
	}

	return resampled;
}


shared_ptr<const AudioBuffers>
Resampler::flush()
{
	auto out = make_shared<AudioBuffers>(_channels, 0);
	int out_offset = 0;
	int64_t const output_size = 65536;

	float dummy[1];
	std::vector<float> buffer(output_size);

	SRC_DATA data;
	data.data_in = dummy;
	data.input_frames = 0;
	data.data_out = buffer.data();
	data.output_frames = output_size;
	data.end_of_input = 1;
	data.src_ratio = double(_out_rate) / _in_rate;

	int const r = src_process(_src, &data);
	if (r) {
		throw EncodeError(fmt::format(N_("could not run sample-rate converter ({})"), src_strerror(r)));
	}

	out->set_frames(out_offset + data.output_frames_gen);

	auto p = data.data_out;
	auto q = out->data();
	for (int i = 0; i < data.output_frames_gen; ++i) {
		for (int j = 0; j < _channels; ++j) {
			q[j][out_offset + i] = *p++;
		}
	}

	out_offset += data.output_frames_gen;

	return out;
}


void
Resampler::reset()
{
	src_reset(_src);
}


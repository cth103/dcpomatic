/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "atmos_reference_signal.h"
#include "audio_buffers.h"
#include "constants.h"
#include <dcp/types.h>
#include <algorithm>


using std::make_shared;
using std::shared_ptr;
using std::vector;


vector<int16_t>
atmos_reference_signal(uint8_t const* data, int size)
{
	if (size < 8 || data[0] != 0x01 || data[5] != 0x11 || data[6] != 0x01 || (data[7] != 0x0e && data[7] != 0x02)) {
		return {};
	}

	uint32_t const length = (uint32_t(data[1]) << 24) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 8) | data[4];
	if (length < 3 || length > uint32_t(size - 5)) {
		return {};
	}

	vector<int16_t> samples((length - 3) / 2);
	for (size_t i = 0; i < samples.size(); ++i) {
		samples[i] = static_cast<int16_t>(data[8 + i * 2] | (data[9 + i * 2] << 8));
	}
	return samples;
}


shared_ptr<AudioBuffers>
atmos_reference_audio(uint8_t const* data, int size, int channels, int channel, int frames)
{
	auto const reference = atmos_reference_signal(data, size);
	if (reference.empty() || frames <= 0 || channel < 0 || channel >= channels) {
		return {};
	}

	auto audio = make_shared<AudioBuffers>(channels, frames);
	audio->make_silent();

	/* Linear interpolation from 19.2kHz up to the film's rate, within this frame only so that each frame's
	 * signal stays where its frame is, whatever is sought to.
	 */
	auto out = audio->data(channel);
	auto const last = static_cast<int>(reference.size()) - 1;
	for (int i = 0; i < frames; ++i) {
		auto const position = static_cast<double>(i) * reference.size() / frames;
		auto const a = std::min(static_cast<int>(position), last);
		auto const b = std::min(a + 1, last);
		auto const t = position - a;
		out[i] = static_cast<float>(((1 - t) * reference[a] + t * reference[b]) / 32768);
	}

	return audio;
}


AudioMapping
atmos_reference_output_mapping(int output_channels)
{
	AudioMapping mapping(MAX_DCP_AUDIO_CHANNELS, output_channels);
	mapping.make_zero();

	auto const centre = static_cast<int>(dcp::Channel::CENTRE);
	if (output_channels == 2) {
		mapping.set(centre, 0, 1);
		mapping.set(centre, 1, 1);
	} else if (output_channels > centre) {
		mapping.set(centre, centre, 1);
	} else if (output_channels == 1) {
		mapping.set(centre, 0, 1);
	}

	return mapping;
}

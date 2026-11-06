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


#include "audio_buffers.h"
#include "audio_util.h"
#include <dcp/sound_frame.h>


using std::make_shared;
using std::shared_ptr;


shared_ptr<AudioBuffers>
deinterleave_sound_frame(shared_ptr<const dcp::SoundFrame> sound_frame, int channels)
{
	auto from = sound_frame->data();
	int const frames = sound_frame->size() / (sound_frame->bits() * channels / 8);
	auto data = make_shared<AudioBuffers>(channels, frames);
	auto data_data = data->data();

	switch (sound_frame->bits()) {
	case 24:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>((from[0] << 8) | (from[1] << 16) | (from[2] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 3;
			}
		}
		break;
	case 16:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>(from[0] << 16 | (from[1] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 2;
			}
		}
		break;
	}

	return data;
}



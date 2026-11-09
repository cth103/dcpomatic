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


#ifndef DCPOMATIC_ATMOS_REFERENCE_SIGNAL_H
#define DCPOMATIC_ATMOS_REFERENCE_SIGNAL_H


#include "audio_mapping.h"
#include <cstdint>
#include <memory>
#include <vector>


class AudioBuffers;


/** Dolby's writers put a mono mix of the programme, at 19.2kHz, in the preamble of each Dolby Atmos frame:
 *  PreambleTag 0x01, a 32-bit big-endian PreambleLength, the bytes 11 01 0E (11 01 02 from Dolby's older
 *  encoders), then 16-bit little-endian samples.  Every certified Dolby Atmos mix has it, and it is there
 *  to check that the Atmos plays and is in sync.
 *  @return the samples, or an empty vector if the frame has no such preamble.
 */
std::vector<int16_t> atmos_reference_signal(uint8_t const* data, int size);


/** @return the reference signal of one Atmos frame as @p frames samples (the frame's length at the
 *  film's audio rate) in channel @p channel of @p channels, every other channel silent; or nullptr if
 *  the frame has no reference signal.
 */
std::shared_ptr<AudioBuffers> atmos_reference_audio(uint8_t const* data, int size, int channels, int channel, int frames);


/** @return a mapping from the DCP channels of a film to @p output_channels outputs which plays only the
 *  centre channel, where atmos_reference_audio() puts the reference signal: to both outputs in stereo,
 *  to the centre with more outputs and to the only output with one.
 */
AudioMapping atmos_reference_output_mapping(int output_channels);


#endif

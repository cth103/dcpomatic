/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
#include "maths_util.h"
#include <dcp/scope_guard.h>
#include <cassert>
#include <cstring>
#include <cmath>


using std::shared_ptr;
using std::make_shared;


/** Construct a silent AudioBuffers */
AudioBuffers::AudioBuffers (int channels, int frames)
{
	allocate (channels, frames);
}


/** Copy constructor.
 *  @param other Other AudioBuffers; data is copied.
 */
AudioBuffers::AudioBuffers (AudioBuffers const & other)
{
	allocate (other.channels(), other.frames());
	copy_from (&other, other.frames(), 0, 0);
}


AudioBuffers::AudioBuffers (std::shared_ptr<const AudioBuffers> other)
{
	allocate (other->channels(), other->frames());
	copy_from (other.get(), other->frames(), 0, 0);
}


AudioBuffers::AudioBuffers (std::shared_ptr<const AudioBuffers> other, int frames_to_copy, int read_offset)
{
	allocate (other->channels(), frames_to_copy);
	copy_from (other.get(), frames_to_copy, read_offset, 0);
}


AudioBuffers &
AudioBuffers::operator= (AudioBuffers const & other)
{
	if (this == &other) {
		return *this;
	}

	allocate (other.channels(), other.frames());
	copy_from (&other, other.frames(), 0, 0);

	return *this;
}


void
AudioBuffers::allocate (int channels, int frames)
{
	DCPOMATIC_ASSERT (frames >= 0);
	DCPOMATIC_ASSERT(frames == 0 || channels > 0);

	dcp::ScopeGuard sg = [this]() { update_data_pointers(); };

	_data.resize(channels);
	for (int channel = 0; channel < channels; ++channel) {
		_data[channel].resize(frames);
	}
}


/** @param channel Channel index.
 *  @return Buffer for this channel.
 */
float*
AudioBuffers::data (int channel)
{
	DCPOMATIC_ASSERT (channel >= 0 && channel < channels());
	return _data[channel].data();
}


/** @param channel Channel index.
 *  @return Buffer for this channel.
 */
float const*
AudioBuffers::data (int channel) const
{
	DCPOMATIC_ASSERT (channel >= 0 && channel < channels());
	return _data[channel].data();
}


/** Set the number of frames in these AudioBuffers */
void
AudioBuffers::set_frames (int frames)
{
	allocate(_data.size(), frames);
}


/** Make all frames silent */
void
AudioBuffers::make_silent ()
{
	for (int channel = 0; channel < channels(); ++channel) {
		make_silent (channel);
	}
}


/** Make all samples on a given channel silent */
void
AudioBuffers::make_silent (int channel)
{
	DCPOMATIC_ASSERT (channel >= 0 && channel < channels());

	/* This isn't really allowed, as all-bits-0 is not guaranteed to mean a 0 float,
	   but it seems that we can get away with it.
	*/
	memset (data(channel), 0, frames() * sizeof(float));
}


/** Make some frames silent.
 *  @param from Start frame.
 */
void
AudioBuffers::make_silent (int from, int frames_to_silence)
{
	DCPOMATIC_ASSERT ((from + frames_to_silence) <= frames());

	for (int channel = 0; channel < channels(); ++channel) {
		/* This isn't really allowed, as all-bits-0 is not guaranteed to mean a 0 float,
		   but it seems that we can get away with it.
		*/
		memset (data(channel) + from, 0, frames_to_silence * sizeof(float));
	}
}


/** Copy data from another AudioBuffers to this one.  All channels are copied.
 *  @param from AudioBuffers to copy from; must have the same number of channels as this.
 *  @param frames_to_copy Number of frames to copy.
 *  @param read_offset Offset to read from in `from'.
 *  @param write_offset Offset to write to in `to'.
 */
void
AudioBuffers::copy_from (AudioBuffers const * from, int frames_to_copy, int read_offset, int write_offset)
{
	if (frames_to_copy == 0) {
		/* Prevent the asserts from firing if there is nothing to do */
		return;
	}

	DCPOMATIC_ASSERT (from);
	DCPOMATIC_ASSERT (from->channels() == channels());
	DCPOMATIC_ASSERT (read_offset >= 0 && (read_offset + frames_to_copy) <= from->frames());
	DCPOMATIC_ASSERT (write_offset >= 0 && (write_offset + frames_to_copy) <= frames());

	for (int channel = 0; channel < channels(); ++channel) {
		memcpy (data(channel) + write_offset, from->data(channel) + read_offset, frames_to_copy * sizeof(float));
	}
}


/** Move audio data around.
 *  @param frames_to_move Number of frames to move.
 *  @param from Offset to move from.
 *  @param to Offset to move to.
 */
void
AudioBuffers::move (int frames_to_move, int from, int to)
{
	if (frames_to_move == 0) {
		return;
	}

	DCPOMATIC_ASSERT (from >= 0);
	DCPOMATIC_ASSERT (from < frames());
	DCPOMATIC_ASSERT (to >= 0);
	DCPOMATIC_ASSERT (to < frames());
	DCPOMATIC_ASSERT (frames_to_move > 0);
	DCPOMATIC_ASSERT (frames_to_move <= frames());
	DCPOMATIC_ASSERT ((from + frames_to_move) <= frames());
	DCPOMATIC_ASSERT ((to + frames_to_move) <= frames());

	for (int channel = 0; channel < channels(); ++channel) {
		memmove (data(channel) + to, data(channel) + from, frames_to_move * sizeof(float));
	}
}


/** Add data from from `from', `from_channel' to our channel `to_channel'.
 *  @param from Buffers to copy data from.
 *  @param from_channel Channel index to read in \p from.
 *  @param to_channel Channel index to accumulate into.
 *  @param gain Linear gain to apply to the data before it is added.
 */
void
AudioBuffers::accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel, float gain)
{
	int const N = frames ();
	DCPOMATIC_ASSERT (from->frames() == N);
	DCPOMATIC_ASSERT (to_channel <= channels());

	auto s = from->data (from_channel);
	auto d = data(to_channel);

	for (int i = 0; i < N; ++i) {
		*d++ += (*s++) * gain;
	}
}


/** Mix some other buffers with these ones.  The AudioBuffers must have the same number of channels.
 *  @param from Audio buffers to get data from.
 *  @param frames Number of frames to mix.
 *  @param read_offset Offset within `from' to read from.
 *  @param write_offset Offset within this to mix into.
 */
void
AudioBuffers::accumulate_frames (AudioBuffers const * from, int frames, int read_offset, int write_offset)
{
	DCPOMATIC_ASSERT (channels() == from->channels());
	DCPOMATIC_ASSERT (read_offset >= 0);
	DCPOMATIC_ASSERT (write_offset >= 0);

	auto from_data = from->data ();
	for (int i = 0; i < channels(); ++i) {
		for (int j = 0; j < frames; ++j) {
			_data[i][j + write_offset] += from_data[i][j + read_offset];
		}
	}
}


/** @param dB gain in dB */
void
AudioBuffers::apply_gain (float dB)
{
	auto const linear = db_to_linear (dB);

	for (int i = 0; i < channels(); ++i) {
		for (int j = 0; j < frames(); ++j) {
			_data[i][j] *= linear;
		}
	}
}


/** @return AudioBuffers object containing only the given channel from this AudioBuffers */
shared_ptr<AudioBuffers>
AudioBuffers::channel (int channel) const
{
	auto output = make_shared<AudioBuffers>(1, frames());
	output->copy_channel_from (this, channel, 0);
	return output;
}


/** Copy all the samples from a channel on another AudioBuffers to a channel on this one.
 *  @param from AudioBuffers to copy from.
 *  @param from_channel Channel index in `from' to copy from.
 *  @param to_channel Channel index in this to copy into, overwriting what's already there.
 */
void
AudioBuffers::copy_channel_from (AudioBuffers const * from, int from_channel, int to_channel)
{
	DCPOMATIC_ASSERT (from->frames() == frames());
	memcpy (data(to_channel), from->data(from_channel), frames() * sizeof (float));
}


/** Make a copy of these AudioBuffers */
shared_ptr<AudioBuffers>
AudioBuffers::clone () const
{
	auto b = make_shared<AudioBuffers>(channels(), frames());
	b->copy_from (this, frames(), 0, 0);
	return b;
}


/** Extend these buffers with the data from another.  The AudioBuffers must have the same number of channels. */
void
AudioBuffers::append (shared_ptr<const AudioBuffers> other)
{
	DCPOMATIC_ASSERT (channels() == other->channels());
	auto old_frames = frames();
	set_frames(old_frames + other->frames());
	copy_from (other.get(), other->frames(), 0, old_frames);
}


/** Remove some frames from the start of these AudioBuffers */
void
AudioBuffers::trim_start (int frames_to_trim)
{
	DCPOMATIC_ASSERT (frames_to_trim <= frames());
	move (frames() - frames_to_trim, frames_to_trim, 0);
	set_frames (frames() - frames_to_trim);
}


void
AudioBuffers::update_data_pointers ()
{
        _data_pointers.resize (channels());
        for (int i = 0; i < channels(); ++i) {
                _data_pointers[i] = _data[i].data();
        }
}


/** Set a new channel count, either discarding data (if new_channels is less than the current
 *  channels()), or filling with silence (if new_channels is more than the current channels()
 */
void
AudioBuffers::set_channels(int new_channels)
{
	DCPOMATIC_ASSERT(new_channels > 0);

	dcp::ScopeGuard sg = [this]() { update_data_pointers(); };

	int const old_channels = channels();
	_data.resize(new_channels);

	for (int channel = old_channels; channel < new_channels; ++channel) {
		_data[channel].resize(frames());
	}
}


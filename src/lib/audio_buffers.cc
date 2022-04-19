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
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>


using std::bad_alloc;
using std::shared_ptr;
using std::make_shared;


/** Construct an AudioBuffers.  Audio data is undefined after this constructor.
 *  @param channels Number of channels.
 *  @param frames Number of frames to reserve space for.
 */
AudioBuffers::AudioBuffers (int channels, int32_t frames)
{
	allocate (channels, frames);
}


/** Copy constructor.
 *  @param other Other AudioBuffers; data is copied.
 */
AudioBuffers::AudioBuffers (AudioBuffers const & other)
{
	allocate (other._channels, other._frames);
	copy_from (&other, other._frames, 0, 0);
}


AudioBuffers::AudioBuffers (std::shared_ptr<const AudioBuffers> other)
{
	allocate (other->_channels, other->_frames);
	copy_from (other.get(), other->_frames, 0, 0);
}


AudioBuffers::AudioBuffers (std::shared_ptr<const AudioBuffers> other, int32_t frames_to_copy, int32_t read_offset)
{
	allocate (other->_channels, frames_to_copy);
	copy_from (other.get(), frames_to_copy, read_offset, 0);
}


AudioBuffers &
AudioBuffers::operator= (AudioBuffers const & other)
{
	if (this == &other) {
		return *this;
	}

	deallocate ();
	allocate (other._channels, other._frames);
	copy_from (&other, other._frames, 0, 0);

	return *this;
}


/** AudioBuffers destructor */
AudioBuffers::~AudioBuffers ()
{
	deallocate ();
}


void
AudioBuffers::allocate (int channels, int32_t frames)
{
	DCPOMATIC_ASSERT (frames >= 0);
	DCPOMATIC_ASSERT (channels >= 0);

	_channels = channels;
	_frames = frames;
	_allocated_frames = frames;

	_data = static_cast<float**> (malloc(_channels * sizeof(float *)));
	if (!_data) {
		throw bad_alloc ();
	}

	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (malloc(frames * sizeof(float)));
		if (!_data[i]) {
			throw bad_alloc ();
		}
	}
}


void
AudioBuffers::deallocate ()
{
	for (int i = 0; i < _channels; ++i) {
		free (_data[i]);
	}

	free (_data);
}


/** @param channel Channel index.
 *  @return Buffer for this channel.
 */
float*
AudioBuffers::data (int channel) const
{
	DCPOMATIC_ASSERT (channel >= 0 && channel < _channels);
	return _data[channel];
}


/** Set the number of frames that these AudioBuffers will report themselves
 *  as having.  If we reduce the number of frames, the `lost' frames will
 *  be silenced.
 *  @param f Frames; must be less than or equal to the number of allocated frames.
 */
void
AudioBuffers::set_frames (int32_t frames)
{
	DCPOMATIC_ASSERT (frames <= _allocated_frames);

	if (frames < _frames) {
		make_silent (frames, _frames - frames);
	}
	_frames = frames;
}


/** Make all frames silent */
void
AudioBuffers::make_silent ()
{
	for (int channel = 0; channel < _channels; ++channel) {
		make_silent (channel);
	}
}


/** Make all samples on a given channel silent */
void
AudioBuffers::make_silent (int channel)
{
	DCPOMATIC_ASSERT (channel >= 0 && channel < _channels);

	/* This isn't really allowed, as all-bits-0 is not guaranteed to mean a 0 float,
	   but it seems that we can get away with it.
	*/
	memset (_data[channel], 0, _frames * sizeof(float));
}


/** Make some frames.
 *  @param from Start frame.
 *  @param frames Number of frames to silence.
 */
void
AudioBuffers::make_silent (int32_t from, int32_t frames)
{
	DCPOMATIC_ASSERT ((from + frames) <= _allocated_frames);

	for (int channel = 0; channel < _channels; ++channel) {
		/* This isn't really allowed, as all-bits-0 is not guaranteed to mean a 0 float,
		   but it seems that we can get away with it.
		*/
		memset (_data[channel] + from, 0, frames * sizeof(float));
	}
}


/** Copy data from another AudioBuffers to this one.  All channels are copied.
 *  @param from AudioBuffers to copy from; must have the same number of channels as this.
 *  @param frames_to_copy Number of frames to copy.
 *  @param read_offset Offset to read from in `from'.
 *  @param write_offset Offset to write to in `to'.
 */
void
AudioBuffers::copy_from (AudioBuffers const * from, int32_t frames_to_copy, int32_t read_offset, int32_t write_offset)
{
	if (frames_to_copy == 0) {
		/* Prevent the asserts from firing if there is nothing to do */
		return;
	}

	DCPOMATIC_ASSERT (from);
	DCPOMATIC_ASSERT (from->channels() == channels());
	DCPOMATIC_ASSERT (read_offset >= 0 && (read_offset + frames_to_copy) <= from->_allocated_frames);
	DCPOMATIC_ASSERT (write_offset >= 0 && (write_offset + frames_to_copy) <= _allocated_frames);

	for (int i = 0; i < _channels; ++i) {
		memcpy (_data[i] + write_offset, from->_data[i] + read_offset, frames_to_copy * sizeof(float));
	}
}


/** Move audio data around.
 *  @param from Offset to move from.
 *  @param to Offset to move to.
 *  @param frames Number of frames to move.
 */
void
AudioBuffers::move (int32_t frames, int32_t from, int32_t to)
{
	if (frames == 0) {
		return;
	}

	DCPOMATIC_ASSERT (from >= 0);
	DCPOMATIC_ASSERT (from < _frames);
	DCPOMATIC_ASSERT (to >= 0);
	DCPOMATIC_ASSERT (to < _frames);
	DCPOMATIC_ASSERT (frames > 0);
	DCPOMATIC_ASSERT (frames <= _frames);
	DCPOMATIC_ASSERT ((from + frames) <= _frames);
	DCPOMATIC_ASSERT ((to + frames) <= _allocated_frames);

	for (int i = 0; i < _channels; ++i) {
		memmove (_data[i] + to, _data[i] + from, frames * sizeof(float));
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
	DCPOMATIC_ASSERT (to_channel <= _channels);

	auto s = from->data (from_channel);
	auto d = _data[to_channel];

	for (int i = 0; i < N; ++i) {
		*d++ += (*s++) * gain;
	}
}


/** Ensure we have space for at least a certain number of frames.  If we extend
 *  the buffers, fill the new space with silence.
 */
void
AudioBuffers::ensure_size (int32_t frames)
{
	if (_allocated_frames >= frames) {
		return;
	}

	/* Round up frames to the next power of 2 to reduce the number
	   of realloc()s that are necessary.
	*/
	frames--;
	frames |= frames >> 1;
	frames |= frames >> 2;
	frames |= frames >> 4;
	frames |= frames >> 8;
	frames |= frames >> 16;
	frames++;

	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (realloc(_data[i], frames * sizeof(float)));
		if (!_data[i]) {
			throw bad_alloc ();
		}
	}

	auto const old_allocated = _allocated_frames;
	_allocated_frames = frames;
	if (old_allocated < _allocated_frames) {
		make_silent (old_allocated, _allocated_frames - old_allocated);
	}
}


/** Mix some other buffers with these ones.  The AudioBuffers must have the same number of channels.
 *  @param from Audio buffers to get data from.
 *  @param frames Number of frames to mix.
 *  @param read_offset Offset within `from' to read from.
 *  @param write_offset Offset within this to mix into.
 */
void
AudioBuffers::accumulate_frames (AudioBuffers const * from, int32_t frames, int32_t read_offset, int32_t write_offset)
{
	DCPOMATIC_ASSERT (_channels == from->channels ());
	DCPOMATIC_ASSERT (read_offset >= 0);
	DCPOMATIC_ASSERT (write_offset >= 0);

	auto from_data = from->data ();
	for (int i = 0; i < _channels; ++i) {
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

	for (int i = 0; i < _channels; ++i) {
		for (int j = 0; j < _frames; ++j) {
			_data[i][j] *= linear;
		}
	}
}


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
	ensure_size (_frames + other->frames());
	copy_from (other.get(), other->frames(), 0, _frames);
	_frames += other->frames();
}


/** Remove some frames from the start of these AudioBuffers */
void
AudioBuffers::trim_start (int32_t frames)
{
	DCPOMATIC_ASSERT (frames <= _frames);
	move (_frames - frames, frames, 0);
	set_frames (_frames - frames);
}

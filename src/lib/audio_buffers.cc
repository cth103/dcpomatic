/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include "audio_buffers.h"
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>

using std::bad_alloc;
using boost::shared_ptr;

/** Construct an AudioBuffers.  Audio data is undefined after this constructor.
 *  @param channels Number of channels.
 *  @param frames Number of frames to reserve space for.
 */
AudioBuffers::AudioBuffers (int channels, int frames)
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

AudioBuffers::AudioBuffers (boost::shared_ptr<const AudioBuffers> other)
{
	allocate (other->_channels, other->_frames);
	copy_from (other.get(), other->_frames, 0, 0);
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
AudioBuffers::allocate (int channels, int frames)
{
	assert (frames >= 0);
	assert (channels >= 0);

	_channels = channels;
	_frames = frames;
	_allocated_frames = frames;
	
	_data = static_cast<float**> (malloc (_channels * sizeof (float *)));
	if (!_data) {
		throw bad_alloc ();
	}
	
	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (malloc (frames * sizeof (float)));
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

/** @param c Channel index.
 *  @return Buffer for this channel.
 */
float*
AudioBuffers::data (int c) const
{
	assert (c >= 0 && c < _channels);
	return _data[c];
}

/** Set the number of frames that these AudioBuffers will report themselves
 *  as having.  If we reduce the number of frames, the `lost' frames will
 *  be silenced.
 *  @param f Frames; must be less than or equal to the number of allocated frames.
 */
void
AudioBuffers::set_frames (int f)
{
	assert (f <= _allocated_frames);

	for (int c = 0; c < _channels; ++c) {
		for (int i = f; i < _frames; ++i) {
			_data[c][i] = 0;
		}
	}
	
	_frames = f;
}

/** Make all samples on all channels silent */
void
AudioBuffers::make_silent ()
{
	for (int i = 0; i < _channels; ++i) {
		make_silent (i);
	}
}

/** Make all samples on a given channel silent.
 *  @param c Channel.
 */
void
AudioBuffers::make_silent (int c)
{
	assert (c >= 0 && c < _channels);
	
	for (int i = 0; i < _frames; ++i) {
		_data[c][i] = 0;
	}
}

void
AudioBuffers::make_silent (int from, int frames)
{
	assert ((from + frames) <= _allocated_frames);

	for (int c = 0; c < _channels; ++c) {
		for (int i = from; i < (from + frames); ++i) {
			_data[c][i] = 0;
		}
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
	
	assert (from->channels() == channels());

	assert (from);
	assert (read_offset >= 0 && (read_offset + frames_to_copy) <= from->_allocated_frames);
	assert (write_offset >= 0 && (write_offset + frames_to_copy) <= _allocated_frames);

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
AudioBuffers::move (int from, int to, int frames)
{
	if (frames == 0) {
		return;
	}
	
	assert (from >= 0);
	assert (from < _frames);
	assert (to >= 0);
	assert (to < _frames);
	assert (frames > 0);
	assert (frames <= _frames);
	assert ((from + frames) <= _frames);
	assert ((to + frames) <= _allocated_frames);
	
	for (int i = 0; i < _channels; ++i) {
		memmove (_data[i] + to, _data[i] + from, frames * sizeof(float));
	}
}

/** Add data from from `from', `from_channel' to our channel `to_channel'.
 *  @param gain Linear gain to apply to the data before it is added.
 */
void
AudioBuffers::accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel, float gain)
{
	int const N = frames ();
	assert (from->frames() == N);
	assert (to_channel <= _channels);

	float* s = from->data (from_channel);
	float* d = _data[to_channel];

	for (int i = 0; i < N; ++i) {
		*d++ += (*s++) * gain;
	}
}

/** Ensure we have space for at least a certain number of frames.  If we extend
 *  the buffers, fill the new space with silence.
 */
void
AudioBuffers::ensure_size (int frames)
{
	if (_allocated_frames >= frames) {
		return;
	}

	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (realloc (_data[i], frames * sizeof (float)));
		if (!_data[i]) {
			throw bad_alloc ();
		}
		for (int j = _allocated_frames; j < frames; ++j) {
			_data[i][j] = 0;
		}
	}

	_allocated_frames = frames;
}

void
AudioBuffers::accumulate_frames (AudioBuffers const * from, int read_offset, int write_offset, int frames)
{
	assert (_channels == from->channels ());
	assert (read_offset >= 0);
	assert (write_offset >= 0);

	for (int i = 0; i < _channels; ++i) {
		for (int j = 0; j < frames; ++j) {
			_data[i][j + write_offset] += from->data()[i][j + read_offset];
		}
	}
}

/** @param dB gain in dB */
void
AudioBuffers::apply_gain (float dB)
{
	float const linear = pow (10, dB / 20);
	
	for (int i = 0; i < _channels; ++i) {
		for (int j = 0; j < _frames; ++j) {
			_data[i][j] *= linear;
		}
	}
}

/** @param c Channel index.
 *  @return AudioBuffers object containing only channel `c' from this AudioBuffers.
 */
shared_ptr<AudioBuffers>
AudioBuffers::channel (int c) const
{
	shared_ptr<AudioBuffers> o (new AudioBuffers (1, frames ()));
	o->copy_channel_from (this, c, 0);
	return o;
}

void
AudioBuffers::copy_channel_from (AudioBuffers const * from, int from_channel, int to_channel)
{
	assert (from->frames() == frames());
	memcpy (data(to_channel), from->data(from_channel), frames() * sizeof (float));
}

shared_ptr<AudioBuffers>
AudioBuffers::clone () const
{
	shared_ptr<AudioBuffers> b (new AudioBuffers (channels (), frames ()));
	b->copy_from (this, frames (), 0, 0);
	return b;
}

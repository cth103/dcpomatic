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

#include <cassert>
#include <cstring>
#include <stdexcept>
#include "audio_buffers.h"

using std::bad_alloc;
using boost::shared_ptr;

/** Construct an AudioBuffers.  Audio data is undefined after this constructor.
 *  @param channels Number of channels.
 *  @param frames Number of frames to reserve space for.
 */
AudioBuffers::AudioBuffers (int channels, int frames)
	: _channels (channels)
	, _frames (frames)
	, _allocated_frames (frames)
{
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

/** Copy constructor.
 *  @param other Other AudioBuffers; data is copied.
 */
AudioBuffers::AudioBuffers (AudioBuffers const & other)
	: _channels (other._channels)
	, _frames (other._frames)
	, _allocated_frames (other._frames)
{
	_data = static_cast<float**> (malloc (_channels * sizeof (float *)));
	if (!_data) {
		throw bad_alloc ();
	}
	
	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (malloc (_frames * sizeof (float)));
		if (!_data[i]) {
			throw bad_alloc ();
		}
		memcpy (_data[i], other._data[i], _frames * sizeof (float));
	}
}

/* XXX: it's a shame that this is a copy-and-paste of the above;
   probably fixable with c++0x.
*/
AudioBuffers::AudioBuffers (boost::shared_ptr<const AudioBuffers> other)
	: _channels (other->_channels)
	, _frames (other->_frames)
	, _allocated_frames (other->_frames)
{
	_data = static_cast<float**> (malloc (_channels * sizeof (float *)));
	if (!_data) {
		throw bad_alloc ();
	}
	
	for (int i = 0; i < _channels; ++i) {
		_data[i] = static_cast<float*> (malloc (_frames * sizeof (float)));
		if (!_data[i]) {
			throw bad_alloc ();
		}
		memcpy (_data[i], other->_data[i], _frames * sizeof (float));
	}
}

/** AudioBuffers destructor */
AudioBuffers::~AudioBuffers ()
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
 *  as having.
 *  @param f Frames; must be less than or equal to the number of allocated frames.
 */
void
AudioBuffers::set_frames (int f)
{
	assert (f <= _allocated_frames);
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

/** Copy data from another AudioBuffers to this one.  All channels are copied.
 *  @param from AudioBuffers to copy from; must have the same number of channels as this.
 *  @param frames_to_copy Number of frames to copy.
 *  @param read_offset Offset to read from in `from'.
 *  @param write_offset Offset to write to in `to'.
 */
void
AudioBuffers::copy_from (AudioBuffers const * from, int frames_to_copy, int read_offset, int write_offset)
{
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
	assert ((to + frames) <= _frames);
	
	for (int i = 0; i < _channels; ++i) {
		memmove (_data[i] + to, _data[i] + from, frames * sizeof(float));
	}
}

/** Add data from from `from', `from_channel' to our channel `to_channel' */
void
AudioBuffers::accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel)
{
	int const N = frames ();
	assert (from->frames() == N);

	float* s = from->data (from_channel);
	float* d = _data[to_channel];

	for (int i = 0; i < N; ++i) {
		*d++ += *s++;
	}
}

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
	}

	_allocated_frames = frames;
}

void
AudioBuffers::accumulate_frames (AudioBuffers const * from, int read_offset, int write_offset, int frames)
{
	assert (_channels == from->channels ());

	for (int i = 0; i < _channels; ++i) {
		for (int j = 0; j < frames; ++j) {
			_data[i][j + write_offset] += from->data()[i][j + read_offset];
		}
	}
}


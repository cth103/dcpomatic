/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/audio_buffers.h
 *  @brief AudioBuffers class.
 */

#ifndef DCPOMATIC_AUDIO_BUFFERS_H
#define DCPOMATIC_AUDIO_BUFFERS_H

#include <boost/shared_ptr.hpp>
#include <stdint.h>

/** @class AudioBuffers
 *  @brief A class to hold multi-channel audio data in float format.
 *
 *  The use of int32_t for frame counts in this class is due to the
 *  round-up to the next power-of-2 code in ::ensure_size; if that
 *  were changed the frame count could use any integer type.
 */
class AudioBuffers
{
public:
	AudioBuffers (int channels, int32_t frames);
	AudioBuffers (AudioBuffers const &);
	AudioBuffers (boost::shared_ptr<const AudioBuffers>);
	~AudioBuffers ();

	AudioBuffers & operator= (AudioBuffers const &);

	boost::shared_ptr<AudioBuffers> clone () const;
	boost::shared_ptr<AudioBuffers> channel (int) const;

	void ensure_size (int32_t);

	float** data () const {
		return _data;
	}

	float* data (int) const;

	int channels () const {
		return _channels;
	}

	int frames () const {
		return _frames;
	}

	void set_frames (int32_t f);

	void make_silent ();
	void make_silent (int c);
	void make_silent (int32_t from, int32_t frames);

	void apply_gain (float);

	void copy_from (AudioBuffers const * from, int32_t frames_to_copy, int32_t read_offset, int32_t write_offset);
	void copy_channel_from (AudioBuffers const * from, int from_channel, int to_channel);
	void move (int32_t frames, int32_t from, int32_t to);
	void accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel, float gain = 1);
	void accumulate_frames (AudioBuffers const * from, int32_t frames, int32_t read_offset, int32_t write_offset);
	void append (boost::shared_ptr<const AudioBuffers> other);
	void trim_start (int32_t frames);

private:
	void allocate (int channels, int32_t frames);
	void deallocate ();

	/** Number of channels */
	int _channels;
	/** Number of frames (where a frame is one sample across all channels) */
	int32_t _frames;
	/** Number of frames that _data can hold */
	int32_t _allocated_frames;
	/** Audio data (so that, e.g. _data[2][6] is channel 2, sample 6) */
	float** _data;
};

#endif

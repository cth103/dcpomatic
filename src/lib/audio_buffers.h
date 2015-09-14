/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/audio_buffers.h
 *  @brief AudioBuffers class.
 */

#ifndef DCPOMATIC_AUDIO_BUFFERS_H
#define DCPOMATIC_AUDIO_BUFFERS_H

#include <boost/shared_ptr.hpp>
#include <stdint.h>

/** @class AudioBuffers
 *  @brief A class to hold multi-channel audio data in float format.
 */
class AudioBuffers
{
public:
	AudioBuffers (int channels, int frames);
	AudioBuffers (AudioBuffers const &);
	AudioBuffers (boost::shared_ptr<const AudioBuffers>);
	~AudioBuffers ();

	AudioBuffers & operator= (AudioBuffers const &);

	boost::shared_ptr<AudioBuffers> clone () const;
	boost::shared_ptr<AudioBuffers> channel (int) const;

	void ensure_size (int);

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

	void set_frames (int f);

	void make_silent ();
	void make_silent (int c);
	void make_silent (int from, int frames);

	void apply_gain (float);

	void copy_from (AudioBuffers const * from, int frames_to_copy, int read_offset, int write_offset);
	void copy_channel_from (AudioBuffers const * from, int from_channel, int to_channel);
	void move (int from, int to, int frames);
	void accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel, float gain = 1);
	void accumulate_frames (AudioBuffers const *, int read_offset, int write_offset, int frames);

private:
	void allocate (int, int);
	void deallocate ();

	/** Number of channels */
	int _channels;
	/** Number of frames (where a frame is one sample across all channels) */
	int _frames;
	/** Number of frames that _data can hold */
	int _allocated_frames;
	/** Audio data (so that, e.g. _data[2][6] is channel 2, sample 6) */
	float** _data;
};

#endif

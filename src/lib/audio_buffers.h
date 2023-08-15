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


/** @file  src/lib/audio_buffers.h
 *  @brief AudioBuffers class.
 */


#ifndef DCPOMATIC_AUDIO_BUFFERS_H
#define DCPOMATIC_AUDIO_BUFFERS_H


#include <memory>
#include <vector>


/** @class AudioBuffers
 *  @brief A class to hold multi-channel audio data in float format.
 */
class AudioBuffers
{
public:
	AudioBuffers (int channels, int frames);
	AudioBuffers (AudioBuffers const &);
	explicit AudioBuffers (std::shared_ptr<const AudioBuffers>);
	AudioBuffers (std::shared_ptr<const AudioBuffers> other, int frames_to_copy, int read_offset);

	AudioBuffers & operator= (AudioBuffers const &);

	std::shared_ptr<AudioBuffers> clone () const;
	std::shared_ptr<AudioBuffers> channel (int) const;

	float* const* data () const {
		return _data_pointers.data();
	}

	float const* data (int) const;
	float* data (int);

	int channels () const {
		return _data.size();
	}

	int frames () const {
		return _data[0].size();
	}

	void set_frames (int f);

	void set_channels(int new_channels);

	void make_silent ();
	void make_silent (int channel);
	void make_silent (int from, int frames);

	void apply_gain (float);

	void copy_from (AudioBuffers const * from, int frames_to_copy, int read_offset, int write_offset);
	void copy_channel_from (AudioBuffers const * from, int from_channel, int to_channel);
	void move (int frames, int from, int to);
	void accumulate_channel (AudioBuffers const * from, int from_channel, int to_channel, float gain = 1);
	void accumulate_frames (AudioBuffers const * from, int frames, int read_offset, int write_offset);
	void append (std::shared_ptr<const AudioBuffers> other);
	void trim_start (int frames);

private:
	void allocate (int channels, int frames);
	void update_data_pointers ();

	/** Audio data (so that, e.g. _data[2][6] is channel 2, sample 6) */
	std::vector<std::vector<float>> _data;
	/** Pointers to the data of each vector in _data */
	std::vector<float*> _data_pointers;
};


#endif

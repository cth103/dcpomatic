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

#include <boost/shared_ptr.hpp>

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

	void copy_from (AudioBuffers const * from, int frames_to_copy, int read_offset, int write_offset);
	void move (int from, int to, int frames);
	void accumulate_channel (AudioBuffers const *, int, int);
	void accumulate_frames (AudioBuffers const *, int read_offset, int write_offset, int frames);

private:
	/** Number of channels */
	int _channels;
	/** Number of frames (where a frame is one sample across all channels) */
	int _frames;
	/** Number of frames that _data can hold */
	int _allocated_frames;
	/** Audio data (so that, e.g. _data[2][6] is channel 2, sample 6) */
	float** _data;
};

/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_AUDIO_RING_BUFFERS_H
#define DCPOMATIC_AUDIO_RING_BUFFERS_H


#include "audio_buffers.h"
#include "types.h"
#include "dcpomatic_time.h"
#include <boost/thread.hpp>
#include <list>


class AudioRingBuffers
{
public:
	AudioRingBuffers ();

	AudioRingBuffers (AudioBuffers const&) = delete;
	AudioRingBuffers& operator= (AudioBuffers const&) = delete;

	void put (std::shared_ptr<const AudioBuffers> data, dcpomatic::DCPTime time, int frame_rate);
	boost::optional<dcpomatic::DCPTime> get (float* out, int channels, int frames);
	boost::optional<dcpomatic::DCPTime> peek () const;

	void clear ();
	/** @return number of frames currently available */
	Frame size () const;

private:
	mutable boost::mutex _mutex;
	std::list<std::pair<std::shared_ptr<const AudioBuffers>, dcpomatic::DCPTime>> _buffers;
	int _used_in_head = 0;
};


#endif

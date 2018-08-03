/*
    Copyright (C) 2016-2017 Carl Hetherington <cth@carlh.net>

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
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <list>

class AudioRingBuffers : public boost::noncopyable
{
public:
	AudioRingBuffers ();

	void put (boost::shared_ptr<const AudioBuffers> data, DCPTime time);
	boost::optional<DCPTime> get (float* out, int channels, int frames);

	void clear ();
	Frame size () const;

private:
	mutable boost::mutex _mutex;
	std::list<std::pair<boost::shared_ptr<const AudioBuffers>, DCPTime> > _buffers;
	int _used_in_head;
};

#endif

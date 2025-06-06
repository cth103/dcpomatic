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


#ifndef DCPOMATIC_VIDEO_RING_BUFFERS_H
#define DCPOMATIC_VIDEO_RING_BUFFERS_H


#include "dcpomatic_time.h"
#include "player_video.h"
#include <boost/thread/mutex.hpp>
#include <utility>


class Film;
class PlayerVideo;


class VideoRingBuffers
{
public:
	VideoRingBuffers() {}

	VideoRingBuffers(VideoRingBuffers const&) = delete;
	VideoRingBuffers& operator=(VideoRingBuffers const&) = delete;

	void put(std::shared_ptr<PlayerVideo> frame, dcpomatic::DCPTime time);
	std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime> get();

	void clear();
	Frame size() const;
	bool empty() const;

	void reset_metadata(std::shared_ptr<const Film> film, dcp::Size player_video_container_size);

	std::pair<size_t, std::string> memory_used() const;

private:
	mutable boost::mutex _mutex;
	std::list<std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime>> _data;
};


#endif

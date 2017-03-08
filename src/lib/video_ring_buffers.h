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

#include "dcpomatic_time.h"
#include "player_video.h"
#include "types.h"
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <utility>

class PlayerVideo;

class VideoRingBuffers : public boost::noncopyable
{
public:
	void put (boost::shared_ptr<PlayerVideo> frame, DCPTime time);
	std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> get ();

	void clear ();
	Frame size () const;
	bool empty () const;
	boost::optional<DCPTime> earliest () const;

private:
	mutable boost::mutex _mutex;
	std::list<std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> > _data;
};

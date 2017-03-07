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

#include "butler.h"
#include "player.h"
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

using std::cout;
using std::pair;
using std::make_pair;
using boost::weak_ptr;
using boost::shared_ptr;
using boost::bind;
using boost::optional;

/** Video readahead in frames */
#define VIDEO_READAHEAD 10

Butler::Butler (weak_ptr<const Film> film, shared_ptr<Player> player)
	: _film (film)
	, _player (player)
	, _pending_seek_accurate (false)
	, _finished (false)
{
	_player->Video.connect (bind (&VideoRingBuffers::put, &_video, _1, _2));
	_thread = new boost::thread (bind (&Butler::thread, this));
}

Butler::~Butler ()
{
	_thread->interrupt ();
	try {
		_thread->join ();
	} catch (boost::thread_interrupted& e) {
		/* No problem */
	}
	delete _thread;
}

void
Butler::thread ()
{
	while (true) {
		boost::mutex::scoped_lock lm (_mutex);

		while (_video.size() > VIDEO_READAHEAD && !_pending_seek_position) {
			_summon.wait (lm);
		}

		if (_pending_seek_position) {
			_player->seek (*_pending_seek_position, _pending_seek_accurate);
			_pending_seek_position = optional<DCPTime> ();
		}

		while (_video.size() < VIDEO_READAHEAD) {
			_arrived.notify_all ();
			if (_player->pass ()) {
				_finished = true;
				break;
			}
		}
	}
}

pair<shared_ptr<PlayerVideo>, DCPTime>
Butler::get_video ()
{
	boost::mutex::scoped_lock lm (_mutex);
	while (_video.size() == 0 && !_finished) {
		_arrived.wait (lm);
	}

	if (_finished) {
		return make_pair (shared_ptr<PlayerVideo>(), DCPTime());
	}

	return _video.get ();
}

void
Butler::seek (DCPTime position, bool accurate)
{
	_video.clear ();

	{
		boost::mutex::scoped_lock lm (_mutex);
		_finished = false;
		_pending_seek_position = position;
		_pending_seek_accurate = accurate;
	}

	_summon.notify_all ();
}

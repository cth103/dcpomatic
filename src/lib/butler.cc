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
/** Audio readahead in frames */
#define AUDIO_READAHEAD (48000*5)

Butler::Butler (weak_ptr<const Film> film, shared_ptr<Player> player, AudioMapping audio_mapping, int audio_channels)
	: _film (film)
	, _player (player)
	, _pending_seek_accurate (false)
	, _finished (false)
	, _died (false)
	, _stop_thread (false)
	, _audio_mapping (audio_mapping)
	, _audio_channels (audio_channels)
	, _disable_audio (false)
{
	_player_video_connection = _player->Video.connect (bind (&Butler::video, this, _1, _2));
	_player_audio_connection = _player->Audio.connect (bind (&Butler::audio, this, _1, _2));
	_player_changed_connection = _player->Changed.connect (bind (&Butler::player_changed, this));
	_thread = new boost::thread (bind (&Butler::thread, this));
}

Butler::~Butler ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_stop_thread = true;
	}

	_thread->interrupt ();
	try {
		_thread->join ();
	} catch (boost::thread_interrupted& e) {
		/* No problem */
	}
	delete _thread;
}

/** Caller must hold a lock on _mutex */
bool
Butler::should_run () const
{
	return (_video.size() < VIDEO_READAHEAD || (!_disable_audio && _audio.size() < AUDIO_READAHEAD)) && !_stop_thread && !_finished && !_died;
}

void
Butler::thread ()
try
{
	while (true) {
		boost::mutex::scoped_lock lm (_mutex);

		/* Wait until we have something to do */
		while (!should_run() && !_pending_seek_position) {
			_summon.wait (lm);
		}

		/* Do any seek that has been requested */
		if (_pending_seek_position) {
			_player->seek (*_pending_seek_position, _pending_seek_accurate);
			_pending_seek_position = optional<DCPTime> ();
		}

		/* Fill _video and _audio.  Don't try to carry on if a pending seek appears
		   while lm is unlocked, as in that state nothing will be added to
		   _video/_audio.
		*/
		while (should_run() && !_pending_seek_position) {
			lm.unlock ();
			bool const r = _player->pass ();
			lm.lock ();
			/* We must check _pending_seek_position again here as it may have been set while lm was unlocked */
			if (r && !_pending_seek_position) {
				_finished = true;
				_arrived.notify_all ();
				break;
			}
			_arrived.notify_all ();
		}
	}
} catch (boost::thread_interrupted) {
	/* The butler thread is being terminated */
	boost::mutex::scoped_lock lm (_mutex);
	_finished = true;
	_arrived.notify_all ();
} catch (...) {
	store_current ();
	boost::mutex::scoped_lock lm (_mutex);
	_died = true;
	_arrived.notify_all ();
}

pair<shared_ptr<PlayerVideo>, DCPTime>
Butler::get_video ()
{
	boost::mutex::scoped_lock lm (_mutex);

	/* Wait for data if we have none */
	while (_video.empty() && !_finished && !_died) {
		_arrived.wait (lm);
	}

	if (_video.empty()) {
		return make_pair (shared_ptr<PlayerVideo>(), DCPTime());
	}

	pair<shared_ptr<PlayerVideo>, DCPTime> const r = _video.get ();
	_summon.notify_all ();
	return r;
}

void
Butler::seek (DCPTime position, bool accurate)
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_died) {
		return;
	}

	_video.clear ();
	_audio.clear ();
	_finished = false;
	_pending_seek_position = position;
	_pending_seek_accurate = accurate;
	_summon.notify_all ();
}

void
Butler::video (shared_ptr<PlayerVideo> video, DCPTime time)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_pending_seek_position) {
			/* Don't store any video while a seek is pending */
			return;
		}
	}

	_video.put (video, time);
}

void
Butler::audio (shared_ptr<AudioBuffers> audio, DCPTime time)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_pending_seek_position || _disable_audio) {
			/* Don't store any audio while a seek is pending, or if audio is disabled */
			return;
		}
	}

	_audio.put (audio, time);
}

void
Butler::player_changed ()
{
	optional<DCPTime> t;

	{
		boost::mutex::scoped_lock lm (_mutex);
		t = _video.earliest ();
	}

	if (t) {
		seek (*t, true);
	} else {
		_video.clear ();
		_audio.clear ();
	}
}

void
Butler::get_audio (float* out, Frame frames)
{
	_audio.get (out, _audio_channels, frames);
	_summon.notify_all ();
}

void
Butler::disable_audio ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_disable_audio = true;
}

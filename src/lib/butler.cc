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
#include "util.h"
#include "log.h"
#include "compose.hpp"
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

using std::cout;
using std::pair;
using std::make_pair;
using boost::weak_ptr;
using boost::shared_ptr;
using boost::bind;
using boost::optional;

/** Minimum video readahead in frames */
#define MINIMUM_VIDEO_READAHEAD 10
/** Maximum video readahead in frames; should never be reached unless there are bugs in Player */
#define MAXIMUM_VIDEO_READAHEAD 240
/** Minimum audio readahead in frames */
#define MINIMUM_AUDIO_READAHEAD (48000*5)
/** Minimum audio readahead in frames; should never be reached unless there are bugs in Player */
#define MAXIMUM_AUDIO_READAHEAD (48000*60)

#define LOG_WARNING(...) _log->log (String::compose(__VA_ARGS__), LogEntry::TYPE_WARNING);

Butler::Butler (shared_ptr<Player> player, shared_ptr<Log> log, AudioMapping audio_mapping, int audio_channels)
	: _player (player)
	, _log (log)
	, _prepare_work (new boost::asio::io_service::work (_prepare_service))
	, _pending_seek_accurate (false)
	, _finished (false)
	, _died (false)
	, _stop_thread (false)
	, _audio_mapping (audio_mapping)
	, _audio_channels (audio_channels)
	, _disable_audio (false)
{
	_player_video_connection = _player->Video.connect (bind (&Butler::video, this, _1, _2));
	_player_audio_connection = _player->Audio.connect (bind (&Butler::audio, this, _1));
	_player_changed_connection = _player->Changed.connect (bind (&Butler::player_changed, this));
	_thread = new boost::thread (bind (&Butler::thread, this));

	/* Create some threads to do work on the PlayerVideos we are creating; at present this is used to
	   multi-thread JPEG2000 decoding.
	*/
	for (size_t i = 0; i < boost::thread::hardware_concurrency(); ++i) {
		_prepare_pool.create_thread (bind (&boost::asio::io_service::run, &_prepare_service));
	}
}

Butler::~Butler ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_stop_thread = true;
	}

	_prepare_work.reset ();
	_prepare_pool.join_all ();
	_prepare_service.stop ();

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
	if (_video.size() >= MAXIMUM_VIDEO_READAHEAD) {
		LOG_WARNING ("Butler video buffers reached %1 frames", _video.size());
	}

	if (_audio.size() >= MAXIMUM_AUDIO_READAHEAD) {
		LOG_WARNING ("Butler audio buffers reached %1 frames", _audio.size());
	}

	return (_video.size() < MINIMUM_VIDEO_READAHEAD || (!_disable_audio && _audio.size() < MINIMUM_AUDIO_READAHEAD))
		&& (_video.size() < MAXIMUM_VIDEO_READAHEAD)
		&& (_audio.size() < MAXIMUM_AUDIO_READAHEAD)
		&& !_stop_thread
		&& !_finished
		&& !_died;
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
			_finished = false;
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
			if (r) {
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
Butler::prepare (weak_ptr<PlayerVideo> weak_video) const
{
	shared_ptr<PlayerVideo> video = weak_video.lock ();
	/* If the weak_ptr cannot be locked the video obviously no longer requires any work */
	if (video) {
		video->prepare ();
	}
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

	_prepare_service.post (bind (&Butler::prepare, this, weak_ptr<PlayerVideo>(video)));
	_video.put (video, time);
}

void
Butler::audio (shared_ptr<AudioBuffers> audio)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_pending_seek_position || _disable_audio) {
			/* Don't store any audio while a seek is pending, or if audio is disabled */
			return;
		}
	}

	_audio.put (remap (audio, _audio_channels, _audio_mapping));
}

void
Butler::player_changed ()
{
	_video.clear ();
	_audio.clear ();
	_summon.notify_all ();
}

/** Try to get `frames' frames of audio and copy it into `out'.  Silence
 *  will be filled if no audio is available.
 *  @return true if there was a buffer underrun, otherwise false.
 */
bool
Butler::get_audio (float* out, Frame frames)
{
	bool const underrun = _audio.get (out, _audio_channels, frames);
	_summon.notify_all ();
	return underrun;
}

void
Butler::disable_audio ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_disable_audio = true;
}

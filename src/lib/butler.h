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

#include "video_ring_buffers.h"
#include "audio_ring_buffers.h"
#include "audio_mapping.h"
#include "exception_store.h"
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/signals2.hpp>
#include <boost/asio.hpp>

class Player;
class PlayerVideo;
class Log;

class Butler : public ExceptionStore, public boost::noncopyable
{
public:
	Butler (boost::shared_ptr<Player> player, boost::shared_ptr<Log> log, AudioMapping map, int audio_channels);
	~Butler ();

	void seek (DCPTime position, bool accurate);
	std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> get_video ();
	boost::optional<DCPTime> get_audio (float* out, Frame frames);

	void disable_audio ();

	std::pair<size_t, std::string> memory_used () const;

private:
	void thread ();
	void video (boost::shared_ptr<PlayerVideo> video, DCPTime time);
	void audio (boost::shared_ptr<AudioBuffers> audio, DCPTime time);
	bool should_run () const;
	void prepare (boost::weak_ptr<PlayerVideo> video) const;
	void player_changed (int);
	void seek_unlocked (DCPTime position, bool accurate);

	boost::shared_ptr<Player> _player;
	boost::shared_ptr<Log> _log;
	boost::thread* _thread;

	/** mutex to protect _video and _audio for when we are clearing them and they both need to be
	    cleared together without any data being inserted in the interim.
	*/
	boost::mutex _video_audio_mutex;
	VideoRingBuffers _video;
	AudioRingBuffers _audio;

	boost::thread_group _prepare_pool;
	boost::asio::io_service _prepare_service;
	boost::shared_ptr<boost::asio::io_service::work> _prepare_work;

	/** mutex to protect _pending_seek_position, _pending_seek_acurate, _finished, _died, _stop_thread */
	boost::mutex _mutex;
	boost::condition _summon;
	boost::condition _arrived;
	boost::optional<DCPTime> _pending_seek_position;
	bool _pending_seek_accurate;
	bool _finished;
	bool _died;
	bool _stop_thread;

	AudioMapping _audio_mapping;
	int _audio_channels;

	bool _disable_audio;

	/** If we are waiting to be refilled following a seek, this is the time we were
	    seeking to.
	*/
	boost::optional<DCPTime> _awaiting;

	boost::signals2::scoped_connection _player_video_connection;
	boost::signals2::scoped_connection _player_audio_connection;
	boost::signals2::scoped_connection _player_changed_connection;
};

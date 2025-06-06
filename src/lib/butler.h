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


#ifndef DCPOMATIC_BUTLER_H
#define DCPOMATIC_BUTLER_H


#include "audio_mapping.h"
#include "audio_ring_buffers.h"
#include "change_signaller.h"
#include "exception_store.h"
#include "io_context.h"
#include "text_ring_buffers.h"
#include "text_type.h"
#include "video_ring_buffers.h"
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>


class Player;
class PlayerVideo;


class Butler : public ExceptionStore
{
public:
	enum class Audio
	{
		ENABLED,
		DISABLED
	};

	Butler(
		std::weak_ptr<const Film> film,
		Player& player,
		AudioMapping map,
		int audio_channels,
		AVPixelFormat pixel_format,
		VideoRange video_range,
		Image::Alignment alignment,
		bool fast,
		bool prepare_only_proxy,
		Audio audio
		);

	~Butler();

	Butler(Butler const&) = delete;
	Butler& operator=(Butler const&) = delete;

	void seek(dcpomatic::DCPTime position, bool accurate);

	class Error {
	public:
		enum class Code {
			NONE,
			AGAIN,
			DIED,
			FINISHED
		};

		Code code = Code::NONE;
		std::string message;

		std::string summary() const;
	};

	enum class Behaviour {
		BLOCKING,
		NON_BLOCKING
	};

	std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime> get_video(Behaviour behaviour, Error* e = nullptr);
	boost::optional<dcpomatic::DCPTime> get_audio(Behaviour behaviour, float* out, Frame frames);
	boost::optional<TextRingBuffers::Data> get_closed_caption();

	std::pair<size_t, std::string> memory_used() const;

private:
	void thread();
	void video(std::shared_ptr<PlayerVideo> video, dcpomatic::DCPTime time);
	void audio(std::shared_ptr<AudioBuffers> audio, dcpomatic::DCPTime time, int frame_rate);
	void text(PlayerText pt, TextType type, boost::optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period);
	bool should_run() const;
	void prepare(std::weak_ptr<PlayerVideo> video);
	void player_change(ChangeType type, int property);
	void seek_unlocked(dcpomatic::DCPTime position, bool accurate);

	std::weak_ptr<const Film> _film;
	Player& _player;
	boost::thread _thread;

	VideoRingBuffers _video;
	AudioRingBuffers _audio;
	TextRingBuffers _closed_caption;

	boost::thread_group _prepare_pool;
	dcpomatic::io_context _prepare_context;
	boost::optional<dcpomatic::work_guard> _prepare_work;

	/** mutex to protect _pending_seek_position, _pending_seek_accurate, _finished, _died, _stop_thread */
	boost::mutex _mutex;
	boost::condition _summon;
	boost::condition _arrived;
	boost::optional<dcpomatic::DCPTime> _pending_seek_position;
	bool _pending_seek_accurate;
	int _suspended;
	bool _finished;
	bool _died;
	std::string _died_message;
	bool _stop_thread;

	AudioMapping _audio_mapping;
	int _audio_channels;

	bool _disable_audio;

	AVPixelFormat _pixel_format;
	VideoRange _video_range;
	Image::Alignment _alignment;
	bool _fast;

	/** true to ask PlayerVideo::prepare to only prepare the ImageProxy and not also
	 *  the final image.  We want to do this when the viewer is intending to call
	 *  PlayerVideo::raw_image() and do the things in PlayerVideo::make_image() itself:
	 *  this is the case for the GLVideoView which can do scale, pixfmt conversion etc.
	 *  in the shader.
	 */
	bool _prepare_only_proxy = false;

	/** If we are waiting to be refilled following a seek, this is the time we were
	    seeking to.
	*/
	boost::optional<dcpomatic::DCPTime> _awaiting;

	boost::signals2::scoped_connection _player_video_connection;
	boost::signals2::scoped_connection _player_audio_connection;
	boost::signals2::scoped_connection _player_text_connection;
	boost::signals2::scoped_connection _player_change_connection;
};


#endif


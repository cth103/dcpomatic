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


#include "butler.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "log.h"
#include "player.h"
#include "util.h"
#include "video_content.h"


using std::cout;
using std::make_pair;
using std::pair;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** Minimum video readahead in frames */
#define MINIMUM_VIDEO_READAHEAD 10
/** Maximum video readahead in frames; should never be exceeded (by much) unless there are bugs in Player */
#define MAXIMUM_VIDEO_READAHEAD 48
/** Minimum audio readahead in frames */
#define MINIMUM_AUDIO_READAHEAD (48000 * MINIMUM_VIDEO_READAHEAD / 24)
/** Maximum audio readahead in frames; should never be exceeded (by much) unless there are bugs in Player */
#define MAXIMUM_AUDIO_READAHEAD (48000 * MAXIMUM_VIDEO_READAHEAD / 24)


/** @param pixel_format Pixel format functor that will be used when calling ::image on PlayerVideos coming out of this
 *  butler.  This will be used (where possible) to prepare the PlayerVideos so that calling image() on them is quick.
 *  @param alignment Same as above for the `alignment' value.
 *  @param fast Same as above for the `fast' flag.
 */
Butler::Butler(
	weak_ptr<const Film> film,
	Player& player,
	AudioMapping audio_mapping,
	int audio_channels,
	AVPixelFormat pixel_format,
	VideoRange video_range,
	Image::Alignment alignment,
	bool fast,
	bool prepare_only_proxy,
	Audio audio
	)
	: _film(film)
	, _player(player)
	, _prepare_work(dcpomatic::make_work_guard(_prepare_context))
	, _pending_seek_accurate(false)
	, _suspended(0)
	, _finished(false)
	, _died(false)
	, _stop_thread(false)
	, _audio_mapping(audio_mapping)
	, _audio_channels(audio_channels)
	, _disable_audio(audio == Audio::DISABLED)
	, _pixel_format(pixel_format)
	, _video_range(video_range)
	, _alignment(alignment)
	, _fast(fast)
	, _prepare_only_proxy(prepare_only_proxy)
{
	_player_video_connection = _player.Video.connect(bind(&Butler::video, this, _1, _2));
	_player_audio_connection = _player.Audio.connect(bind(&Butler::audio, this, _1, _2, _3));
	_player_text_connection = _player.Text.connect(bind(&Butler::text, this, _1, _2, _3, _4));
	/* The butler must hear about things first, otherwise it might not sort out suspensions in time for
	   get_video() to be called in response to this signal.
	*/
	_player_change_connection = _player.Change.connect(bind(&Butler::player_change, this, _1, _2, _3), boost::signals2::at_front);
	_thread = boost::thread(bind(&Butler::thread, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np(_thread.native_handle(), "butler");
#endif

	/* Create some threads to do work on the PlayerVideos we are creating; at present this is used to
	   multi-thread JPEG2000 decoding.
	*/

	LOG_TIMING("start-prepare-threads {}", boost::thread::hardware_concurrency() * 2);

	for (size_t i = 0; i < boost::thread::hardware_concurrency() * 2; ++i) {
		_prepare_pool.create_thread(bind(&dcpomatic::io_context::run, &_prepare_context));
	}
}


Butler::~Butler()
{
	boost::this_thread::disable_interruption dis;

	{
		boost::mutex::scoped_lock lm(_mutex);
		_stop_thread = true;
	}

	_prepare_work.reset();
	_prepare_pool.join_all();
	_prepare_context.stop();

	_thread.interrupt();
	try {
		_thread.join();
	} catch (...) {}
}

/** Caller must hold a lock on _mutex */
bool
Butler::should_run() const
{
	LOG_DEBUG_BUTLER("BUT: video={} audio={}", _video.size(), _audio.size());

	if (_video.size() >= MAXIMUM_VIDEO_READAHEAD * 10) {
		/* This is way too big */
		auto pos = _audio.peek();
		if (pos) {
			throw ProgrammingError
				(__FILE__, __LINE__, fmt::format("Butler video buffers reached {} frames (audio is {} at {})", _video.size(), _audio.size(), pos->get()));
		} else {
			throw ProgrammingError
				(__FILE__, __LINE__, fmt::format("Butler video buffers reached {} frames (audio is {})", _video.size(), _audio.size()));
		}
	}

	if (_audio.size() >= MAXIMUM_AUDIO_READAHEAD * 10) {
		/* This is way too big */
		auto pos = _audio.peek();
		if (pos) {
			throw ProgrammingError
				(__FILE__, __LINE__, fmt::format("Butler audio buffers reached {} frames at {} (video is {})", _audio.size(), pos->get(), _video.size()));
		} else {
			throw ProgrammingError
				(__FILE__, __LINE__, fmt::format("Butler audio buffers reached {} frames (video is {})", _audio.size(), _video.size()));
		}
	}

	if (_video.size() >= MAXIMUM_VIDEO_READAHEAD * 2) {
		LOG_WARNING("Butler video buffers reached {} frames (audio is {})", _video.size(), _audio.size());
	}

	if (_audio.size() >= MAXIMUM_AUDIO_READAHEAD * 2) {
		LOG_WARNING("Butler audio buffers reached {} frames (video is {})", _audio.size(), _video.size());
	}

	if (_stop_thread || _finished || _died || _suspended) {
		/* Definitely do not run */
		return false;
	}

	if (_video.size() < MINIMUM_VIDEO_READAHEAD || (!_disable_audio && _audio.size() < MINIMUM_AUDIO_READAHEAD)) {
		/* Definitely do run: we need data */
		return true;
	}

	/* Run if we aren't full of video or audio */
	return (_video.size() < MAXIMUM_VIDEO_READAHEAD) && (_audio.size() < MAXIMUM_AUDIO_READAHEAD);
}


void
Butler::thread()
try
{
	start_of_thread("Butler");

	while (true) {
		boost::mutex::scoped_lock lm(_mutex);

		/* Wait until we have something to do */
		while (!should_run() && !_pending_seek_position) {
			_summon.wait(lm);
		}

		/* Do any seek that has been requested */
		if (_pending_seek_position) {
			_finished = false;
			_player.seek(*_pending_seek_position, _pending_seek_accurate);
			_pending_seek_position = optional<DCPTime>();
		}

		/* Fill _video and _audio.  Don't try to carry on if a pending seek appears
		   while lm is unlocked, as in that state nothing will be added to
		   _video/_audio.
		*/
		while (should_run() && !_pending_seek_position) {
			lm.unlock();
			bool const r = _player.pass();
			lm.lock();
			if (r) {
				_finished = true;
				_arrived.notify_all();
				break;
			}
			_arrived.notify_all();
		}
	}
} catch (boost::thread_interrupted) {
	/* The butler thread is being terminated */
	boost::mutex::scoped_lock lm(_mutex);
	_finished = true;
	_arrived.notify_all();
} catch (std::exception& e) {
	store_current();
	boost::mutex::scoped_lock lm(_mutex);
	_died = true;
	_died_message = e.what();
	_arrived.notify_all();
} catch (...) {
	store_current();
	boost::mutex::scoped_lock lm(_mutex);
	_died = true;
	_arrived.notify_all();
}


/** @param behaviour BLOCKING if we should block until video is available.  If behaviour is NON_BLOCKING
 *  and no video is immediately available the method will return a 0 PlayerVideo and the error AGAIN.
 *  @param e if non-0 this is filled with an error code (if an error occurs) or is untouched if no error occurs.
 */
pair<shared_ptr<PlayerVideo>, DCPTime>
Butler::get_video(Behaviour behaviour, Error* e)
{
	boost::mutex::scoped_lock lm(_mutex);

	auto setup_error = [this](Error* e, Error::Code fallback) {
		if (e) {
			if (_died) {
				e->code = Error::Code::DIED;
				e->message = _died_message;
			} else if (_finished) {
				e->code = Error::Code::FINISHED;
			} else {
				e->code = fallback;
			}
		}
	};

	if (_video.empty() && (_finished || _died || (_suspended && behaviour == Behaviour::NON_BLOCKING))) {
		setup_error(e, Error::Code::AGAIN);
		return make_pair(shared_ptr<PlayerVideo>(), DCPTime());
	}

	/* Wait for data if we have none */
	while (_video.empty() && !_finished && !_died) {
		_arrived.wait(lm);
	}

	if (_video.empty()) {
		setup_error(e, Error::Code::NONE);
		return make_pair(shared_ptr<PlayerVideo>(), DCPTime());
	}

	auto const r = _video.get();
	_summon.notify_all();
	return r;
}


optional<TextRingBuffers::Data>
Butler::get_closed_caption()
{
	boost::mutex::scoped_lock lm(_mutex);
	return _closed_caption.get();
}


void
Butler::seek(DCPTime position, bool accurate)
{
	boost::mutex::scoped_lock lm(_mutex);
	_awaiting = optional<DCPTime>();
	seek_unlocked(position, accurate);
}


void
Butler::seek_unlocked(DCPTime position, bool accurate)
{
	if (_died) {
		return;
	}

	_finished = false;
	_pending_seek_position = position;
	_pending_seek_accurate = accurate;

	_video.clear();
	_audio.clear();
	_closed_caption.clear();

	_summon.notify_all();
}


void
Butler::prepare(weak_ptr<PlayerVideo> weak_video)
try
{
	auto video = weak_video.lock();
	/* If the weak_ptr cannot be locked the video obviously no longer requires any work */
	if (video) {
		LOG_TIMING("start-prepare in {}", thread_id());
		video->prepare(_pixel_format, _video_range, _alignment, _fast, _prepare_only_proxy);
		LOG_TIMING("finish-prepare in {}", thread_id());
	}
}
catch (std::exception& e)
{
	store_current();
	boost::mutex::scoped_lock lm(_mutex);
	_died = true;
	_died_message = e.what();
}
catch (...)
{
	store_current();
	boost::mutex::scoped_lock lm(_mutex);
	_died = true;
}


void
Butler::video(shared_ptr<PlayerVideo> video, DCPTime time)
{
	boost::mutex::scoped_lock lm(_mutex);

	if (_pending_seek_position) {
		/* Don't store any video in this case */
		return;
	}

	dcpomatic::post(_prepare_context, bind(&Butler::prepare, this, weak_ptr<PlayerVideo>(video)));

	_video.put(video, time);
}


void
Butler::audio(shared_ptr<AudioBuffers> audio, DCPTime time, int frame_rate)
{
	boost::mutex::scoped_lock lm(_mutex);
	if (_pending_seek_position || _disable_audio) {
		/* Don't store any audio in these cases */
		return;
	}

	_audio.put(remap(audio, _audio_channels, _audio_mapping), time, frame_rate);
}


/** Try to get `frames' frames of audio and copy it into `out'.
 *  @param behaviour BLOCKING if we should block until audio is available.  If behaviour is NON_BLOCKING
 *  and no audio is immediately available the buffer will be filled with silence and boost::none
 *  will be returned.
 *  @return time of this audio, or unset if blocking was false and no data was available.
 */
optional<DCPTime>
Butler::get_audio(Behaviour behaviour, float* out, Frame frames)
{
	boost::mutex::scoped_lock lm(_mutex);

	while (behaviour == Behaviour::BLOCKING && !_finished && !_died && _audio.size() < frames) {
		_arrived.wait(lm);
	}

	auto t = _audio.get(out, _audio_channels, frames);
	_summon.notify_all();
	return t;
}


pair<size_t, string>
Butler::memory_used() const
{
	/* XXX: should also look at _audio.memory_used() */
	return _video.memory_used();
}


void
Butler::player_change(ChangeType type, int property, bool frequent)
{
	if (frequent) {
		return;
	}

	if (property == VideoContentProperty::CROP) {
		if (type == ChangeType::DONE) {
			auto film = _film.lock();
			if (film) {
				_video.reset_metadata(film, _player.video_container_size());
			}
		}
		return;
	}

	boost::mutex::scoped_lock lm(_mutex);

	if (type == ChangeType::PENDING) {
		++_suspended;
	} else if (type == ChangeType::DONE) {
		--_suspended;
		if (_died || _pending_seek_position) {
			lm.unlock();
			_summon.notify_all();
			return;
		}

		DCPTime seek_to;
		auto next = _video.get().second;
		if (_awaiting && _awaiting > next) {
			/* We have recently done a player_changed seek and our buffers haven't been refilled yet,
			   so assume that we're seeking to the same place as last time.
			*/
			seek_to = *_awaiting;
		} else {
			seek_to = next;
		}

		seek_unlocked(seek_to, true);
		_awaiting = seek_to;
	} else if (type == ChangeType::CANCELLED) {
		--_suspended;
	}

	lm.unlock();
	_summon.notify_all();
}


void
Butler::text(PlayerText pt, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	if (type != TextType::CLOSED_CAPTION) {
		return;
	}

	DCPOMATIC_ASSERT(track);

	_closed_caption.put(pt, *track, period);
}


string
Butler::Error::summary() const
{
	switch (code)
	{
		case Error::Code::NONE:
			return "No error registered";
		case Error::Code::AGAIN:
			return "Butler not ready";
		case Error::Code::DIED:
			return fmt::format("Butler died ({})", message);
		case Error::Code::FINISHED:
			return "Butler finished";
	}

	return "";
}


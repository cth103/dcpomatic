/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_VIDEO_VIEW_H
#define DCPOMATIC_VIDEO_VIEW_H

#include "lib/dcpomatic_time.h"
#include "lib/timer.h"
#include "lib/types.h"
#include "lib/exception_store.h"
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

class Image;
class wxWindow;
class FilmViewer;
class Player;
class PlayerVideo;

class VideoView : public ExceptionStore, public boost::noncopyable
{
public:
	VideoView (FilmViewer* viewer);
	virtual ~VideoView () {}

	/** @return the thing displaying the image */
	virtual wxWindow* get () const = 0;
	/** Re-make and display the image from the current _player_video */
	virtual void update () = 0;
	/** Called when playback starts */
	virtual void start ();
	/** Called when playback stops */
	virtual void stop () {}

	enum NextFrameResult {
		FAIL,
		AGAIN,
		SUCCESS
	};

	/** Get the next frame and display it; used after seek */
	virtual NextFrameResult display_next_frame (bool) = 0;

	void clear ();
	bool reset_metadata (std::shared_ptr<const Film> film, dcp::Size player_video_container_size);

	/** Emitted from the GUI thread when our display changes in size */
	boost::signals2::signal<void()> Sized;


	/* Accessors for FilmViewer */

	int dropped () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _dropped;
	}

	int errored () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _errored;
	}

	int gets () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _gets;
	}

	StateTimer const & state_timer () const {
		return _state_timer;
	}

	dcpomatic::DCPTime position () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _player_video.second;
	}


	/* Setters for FilmViewer so it can tell us our state and
	 * we can then use (thread) safely.
	 */

	void set_video_frame_rate (int r) {
		boost::mutex::scoped_lock lm (_mutex);
		_video_frame_rate = r;
	}

	void set_length (dcpomatic::DCPTime len) {
		boost::mutex::scoped_lock lm (_mutex);
		_length = len;
	}

	void set_eyes (Eyes eyes) {
		boost::mutex::scoped_lock lm (_mutex);
		_eyes = eyes;
	}

	void set_three_d (bool t) {
		boost::mutex::scoped_lock lm (_mutex);
		_three_d = t;
	}

protected:
	NextFrameResult get_next_frame (bool non_blocking);
	boost::optional<int> time_until_next_frame () const;
	dcpomatic::DCPTime one_video_frame () const;

	int video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_rate;
	}

	dcpomatic::DCPTime length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _length;
	}

	std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime> player_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _player_video;
	}

	void add_dropped () {
		boost::mutex::scoped_lock lm (_mutex);
		++_dropped;
	}

	void add_get () {
		boost::mutex::scoped_lock lm (_mutex);
		++_gets;
	}

	FilmViewer* _viewer;

	StateTimer _state_timer;

private:
	/** Mutex protecting all the state in this class */
	mutable boost::mutex _mutex;

	std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime> _player_video;
	int _video_frame_rate;
	/** length of the film we are playing, or 0 if there is none */
	dcpomatic::DCPTime _length;
	Eyes _eyes;
	bool _three_d;

	int _dropped;
	int _errored;
	int _gets;
};

#endif

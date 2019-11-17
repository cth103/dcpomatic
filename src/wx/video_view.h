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
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>

class Image;
class wxWindow;
class FilmViewer;
class PlayerVideo;

class VideoView
{
public:
	VideoView (FilmViewer* viewer)
		: _viewer (viewer)
#ifdef DCPOMATIC_VARIANT_SWAROOP
		, _in_watermark (false)
#endif
	{}

	virtual ~VideoView () {}

	virtual void set_image (boost::shared_ptr<const Image> image) = 0;
	virtual wxWindow* get () const = 0;
	virtual void update () = 0;

	/* XXX_b: make pure */
	virtual void start () {}
	/* XXX_b: make pure */
	virtual void stop () {}

	void clear ();

	boost::signals2::signal<void()> Sized;

	virtual bool display_next_frame (bool) = 0;

	/* XXX_b: to remove */
	virtual void display_player_video () {}

	dcpomatic::DCPTime position () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _player_video.second;
	}

	void set_film (boost::shared_ptr<const Film> film) {
		boost::mutex::scoped_lock lm (_mutex);
		_film = film;
	}

protected:
	/* XXX_b: to remove */
	friend class FilmViewer;

	bool get_next_frame (bool non_blocking);
	int time_until_next_frame () const;
	dcpomatic::DCPTime one_video_frame () const;

	boost::shared_ptr<const Film> film () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _film;
	}

	FilmViewer* _viewer;
	std::pair<boost::shared_ptr<PlayerVideo>, dcpomatic::DCPTime> _player_video;

	/** Mutex protecting all the state in VideoView */
	mutable boost::mutex _mutex;

#ifdef DCPOMATIC_VARIANT_SWAROOP
	bool _in_watermark;
	int _watermark_x;
	int _watermark_y;
#endif

private:
	boost::shared_ptr<const Film> _film;
};

#endif

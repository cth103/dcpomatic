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

#include "video_view.h"
#include "wx_util.h"
#include "film_viewer.h"
#include "lib/butler.h"
#include "lib/dcpomatic_log.h"
#include <boost/optional.hpp>

using std::pair;
using boost::shared_ptr;
using boost::optional;

VideoView::VideoView (FilmViewer* viewer)
	: _viewer (viewer)
	, _state_timer ("viewer")
	, _video_frame_rate (0)
	, _eyes (EYES_LEFT)
	, _three_d (false)
	, _dropped (0)
	, _errored (0)
	, _gets (0)
{

}

void
VideoView::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_player_video.first.reset ();
	_player_video.second = dcpomatic::DCPTime ();
}

/** Could be called from any thread.
 *  @param non_blocking true to return false quickly if no video is available quickly.
 *  @return FAIL if there's no frame, AGAIN if the method should be called again, or SUCCESS
 *  if there is a frame.
 */
VideoView::NextFrameResult
VideoView::get_next_frame (bool non_blocking)
{
	if (length() == dcpomatic::DCPTime()) {
		return FAIL;
	}

	shared_ptr<Butler> butler = _viewer->butler ();
	if (!butler) {
		return FAIL;
	}
	add_get ();

	boost::mutex::scoped_lock lm (_mutex);

	do {
		Butler::Error e;
		pair<shared_ptr<PlayerVideo>, dcpomatic::DCPTime> pv = butler->get_video (!non_blocking, &e);
		if (e.code == Butler::Error::DIED) {
			LOG_ERROR ("Butler died with %1", e.summary());
		}
		if (!pv.first) {
			return e.code == Butler::Error::AGAIN ? AGAIN : FAIL;
		}
		_player_video = pv;
	} while (
		_player_video.first &&
		_three_d &&
		_eyes != _player_video.first->eyes() &&
		_player_video.first->eyes() != EYES_BOTH
		);

	if (_player_video.first && _player_video.first->error()) {
		++_errored;
	}

	return SUCCESS;
}

dcpomatic::DCPTime
VideoView::one_video_frame () const
{
	return dcpomatic::DCPTime::from_frames (1, video_frame_rate());
}

/** @return Time in ms until the next frame is due, or empty if nothing is due */
optional<int>
VideoView::time_until_next_frame () const
{
	if (length() == dcpomatic::DCPTime()) {
		/* There's no content, so this doesn't matter */
		return optional<int>();
	}

	dcpomatic::DCPTime const next = position() + one_video_frame();
	dcpomatic::DCPTime const time = _viewer->audio_time().get_value_or(position());
	if (next < time) {
		return 0;
	}
	return (next.seconds() - time.seconds()) * 1000;
}

void
VideoView::start ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_dropped = 0;
	_errored = 0;
}

bool
VideoView::reset_metadata (shared_ptr<const Film> film, dcp::Size player_video_container_size)
{
	pair<shared_ptr<PlayerVideo>, dcpomatic::DCPTime> pv = player_video ();
	if (!pv.first) {
		return false;
	}

	if (!pv.first->reset_metadata(film, player_video_container_size)) {
		return false;
	}

	update ();
	return true;
}


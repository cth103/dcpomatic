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

void
VideoView::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_player_video.first.reset ();
	_player_video.second = dcpomatic::DCPTime ();
}

/** @param non_blocking true to return false quickly if no video is available quickly.
 *  @return false if we gave up because it would take too long, otherwise true.
 */
bool
VideoView::get_next_frame (bool non_blocking)
{
	DCPOMATIC_ASSERT (_viewer->butler());
	_viewer->_gets++;

	boost::mutex::scoped_lock lm (_mutex);

	do {
		Butler::Error e;
		_player_video = _viewer->butler()->get_video (!non_blocking, &e);
		if (!_player_video.first && e == Butler::AGAIN) {
			return false;
		}
	} while (
		_player_video.first &&
		_viewer->film()->three_d() &&
		_viewer->_eyes != _player_video.first->eyes() &&
		_player_video.first->eyes() != EYES_BOTH
		);

	try {
		_viewer->butler()->rethrow ();
	} catch (DecodeError& e) {
		error_dialog (get(), e.what());
	}

	return true;
}

dcpomatic::DCPTime
VideoView::one_video_frame () const
{
	return dcpomatic::DCPTime::from_frames (1, film()->video_frame_rate());
}

/* XXX_b: comment */
int
VideoView::time_until_next_frame () const
{
	dcpomatic::DCPTime const next = position() + one_video_frame();
	dcpomatic::DCPTime const time = _viewer->audio_time().get_value_or(position());
	std::cout << to_string(next) << " " << to_string(time) << " " << ((next.seconds() - time.seconds()) * 1000) << "\n";
	if (next < time) {
		return 0;
	}
	return (next.seconds() - time.seconds()) * 1000;
}


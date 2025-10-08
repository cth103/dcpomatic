/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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
#include <sys/time.h>


using std::shared_ptr;
using boost::optional;


static constexpr int TOO_MANY_DROPPED_FRAMES = 20;
static constexpr int TOO_MANY_DROPPED_PERIOD = 5.0;


VideoView::VideoView(FilmViewer* viewer)
	: _viewer(viewer)
	, _state_timer("viewer")
{

}


void
VideoView::clear()
{
	boost::mutex::scoped_lock lm(_mutex);
	_player_video.first.reset();
	_player_video.second = dcpomatic::DCPTime();
}


/** Could be called from any thread.
 *  @param non_blocking true to return false quickly if no video is available quickly.
 *  @return FAIL if there's no frame, AGAIN if the method should be called again, or SUCCESS
 *  if there is a frame.
 */
VideoView::NextFrameResult
VideoView::get_next_frame(bool non_blocking)
{
	if (length() == dcpomatic::DCPTime()) {
		return FAIL;
	}

	auto butler = _viewer->butler();
	if (!butler) {
		return FAIL;
	}
	add_get();

	boost::mutex::scoped_lock lm(_mutex);

	do {
		Butler::Error e;
		auto pv = butler->get_video(non_blocking ? Butler::Behaviour::NON_BLOCKING : Butler::Behaviour::BLOCKING, &e);
		if (e.code == Butler::Error::Code::DIED) {
			LOG_ERROR("Butler died with {}", e.summary());
		}
		if (!pv.first) {
			return e.code == Butler::Error::Code::AGAIN ? AGAIN : FAIL;
		}
		_player_video = pv;
	} while (
		_player_video.first &&
		_three_d &&
		_eyes != _player_video.first->eyes() &&
		_player_video.first->eyes() != Eyes::BOTH
		);

	if (_player_video.first && _player_video.first->error()) {
		++_errored;
	}

	return SUCCESS;
}


dcpomatic::DCPTime
VideoView::one_video_frame() const
{
	return dcpomatic::DCPTime::from_frames(1, video_frame_rate());
}


/** @return Time in ms until the next frame is due, or empty if nothing is due */
optional<int>
VideoView::time_until_next_frame() const
{
	if (length() == dcpomatic::DCPTime()) {
		/* There's no content, so this doesn't matter */
		return {};
	}

	auto const next = position() + one_video_frame();
	auto const time = _viewer->audio_time().get_value_or(position());
	if (next < time) {
		return 0;
	}
	return (next.seconds() - time.seconds()) * 1000;
}


void
VideoView::start()
{
	boost::mutex::scoped_lock lm(_mutex);
	_dropped = 0;
	_errored = 0;
	gettimeofday(&_dropped_check_period_start, nullptr);
}


bool
VideoView::reset_metadata(shared_ptr<const Film> film, dcp::Size player_video_container_size)
{
	auto pv = player_video();
	if (!pv.first) {
		return false;
	}

	if (!pv.first->reset_metadata(film, player_video_container_size)) {
		return false;
	}

	update();
	return true;
}


void
VideoView::add_dropped()
{
	bool too_many = false;

	{
		boost::mutex::scoped_lock lm(_mutex);
		++_dropped;
		if (_dropped > TOO_MANY_DROPPED_FRAMES) {
			struct timeval now;
			gettimeofday(&now, nullptr);
			double const elapsed = seconds(now) - seconds(_dropped_check_period_start);
			too_many = elapsed < TOO_MANY_DROPPED_PERIOD;
			_dropped = 0;
			_dropped_check_period_start = now;
		}
	}

	if (too_many) {
		emit(boost::bind(boost::ref(TooManyDropped)));
	}
}


wxColour
VideoView::pad_colour() const
{
	if (_viewer->pad_black()) {
		return wxColour(0, 0, 0);
	} else if (gui_is_dark()) {
		return wxColour(50, 50, 50);
	} else {
		return wxColour(240, 240, 240);
	}
}

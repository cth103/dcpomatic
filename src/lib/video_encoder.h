/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_VIDEO_ENCODER_H
#define DCPOMATIC_VIDEO_ENCODER_H


#include "dcpomatic_time.h"
#include "event_history.h"
#include "film.h"
#include "player_video.h"


class Writer;


class VideoEncoder
{
public:
	VideoEncoder(std::shared_ptr<const Film> film, Writer& writer);
	virtual ~VideoEncoder() {}

	VideoEncoder(VideoEncoder const&) = delete;
	VideoEncoder& operator=(VideoEncoder const&) = delete;

	/** Called to indicate that a processing run is about to begin */
	virtual void begin() {}

	/** Called to pass a bit of video to be encoded as the next DCP frame */
	virtual void encode(std::shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time);

	virtual void pause() = 0;
	virtual void resume() = 0;

	/** Called when a processing run has finished */
	virtual void end() = 0;

	int video_frames_enqueued() const;
	int video_frames_encoded() const;
	boost::optional<float> current_encoding_rate() const;

protected:
	/** Film that we are encoding */
	std::shared_ptr<const Film> _film;
	Writer& _writer;
	EventHistory _history;
	boost::optional<dcpomatic::DCPTime> _last_player_video_time;
};


#endif


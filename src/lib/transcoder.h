/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TRANSCODER_H
#define DCPOMATIC_TRANSCODER_H

#include "types.h"
#include "player_subtitles.h"
#include <boost/weak_ptr.hpp>

class Film;
class Encoder;
class Player;
class Job;
class PlayerVideo;
class AudioBuffers;

/** @class Transcoder */
class Transcoder : public boost::noncopyable
{
public:
	Transcoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job);
	virtual ~Transcoder () {}

	virtual void go () = 0;

	virtual float current_encoding_rate () const = 0;
	virtual int video_frames_enqueued () const = 0;

protected:
	virtual void video (boost::shared_ptr<PlayerVideo>, DCPTime) = 0;
	virtual void audio (boost::shared_ptr<AudioBuffers>, DCPTime) = 0;
	virtual void subtitle (PlayerSubtitles, DCPTimePeriod) = 0;

	boost::shared_ptr<const Film> _film;
	boost::weak_ptr<Job> _job;
	boost::shared_ptr<Player> _player;
};

#endif

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

#ifndef DCPOMATIC_ENCODER_H
#define DCPOMATIC_ENCODER_H

#include "types.h"
#include "player_caption.h"
#include <boost/weak_ptr.hpp>
#include <boost/signals2.hpp>

class Film;
class Encoder;
class Player;
class Job;
class PlayerVideo;
class AudioBuffers;

/** @class Encoder */
class Encoder : public boost::noncopyable
{
public:
	Encoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job);
	virtual ~Encoder () {}

	virtual void go () = 0;

	/** @return the current frame rate over the last short while */
	virtual float current_rate () const = 0;
	/** @return the number of frames that are done */
	virtual Frame frames_done () const = 0;
	virtual bool finishing () const = 0;

protected:
	boost::shared_ptr<const Film> _film;
	boost::weak_ptr<Job> _job;
	boost::shared_ptr<Player> _player;
};

#endif

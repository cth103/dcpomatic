/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FILM_ENCODER_H
#define DCPOMATIC_FILM_ENCODER_H


#include "player.h"
#include "player_text.h"
LIBDCP_DISABLE_WARNINGS
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS


class Film;
class Player;
class Job;
class PlayerVideo;
class AudioBuffers;


/** @class FilmEncoder
 *  @brief Parent class for something that can encode a film into some format
 */
class FilmEncoder
{
public:
	FilmEncoder(std::shared_ptr<const Film> film, std::weak_ptr<Job> job);
	virtual ~FilmEncoder() {}

	FilmEncoder(FilmEncoder const&) = delete;
	FilmEncoder& operator=(FilmEncoder const&) = delete;

	virtual void go () = 0;

	/** @return the current frame rate over the last short while */
	virtual boost::optional<float> current_rate () const {
		return {};
	}

	/** @return the number of frames that are done */
	virtual Frame frames_done () const = 0;
	virtual bool finishing () const = 0;
	virtual void pause() {}
	virtual void resume() {}

protected:
	std::shared_ptr<const Film> _film;
	std::weak_ptr<Job> _job;
	Player _player;
};


#endif

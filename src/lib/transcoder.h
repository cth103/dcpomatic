/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "types.h"

class Film;
class Encoder;
class Player;
class Writer;
class Job;

/** @class Transcoder */
class Transcoder : public boost::noncopyable
{
public:
	Transcoder (boost::shared_ptr<const Film>, boost::shared_ptr<Job>);

	void go ();

	float current_encoding_rate () const;
	int video_frames_out () const;

	/** @return true if we are in the process of calling Encoder::process_end */
	bool finishing () const {
		return _finishing;
	}

private:
	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<Player> _player;
	boost::shared_ptr<Writer> _writer;
	boost::shared_ptr<Encoder> _encoder;
	bool _finishing;
};

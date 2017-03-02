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
#include "player_subtitles.h"
#include <boost/weak_ptr.hpp>

class Film;
class Encoder;
class Player;
class Writer;
class Job;
class PlayerVideo;
class AudioBuffers;

/** @class Transcoder */
class Transcoder : public boost::noncopyable
{
public:
	Transcoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job);

	void go ();

	float current_encoding_rate () const;
	int video_frames_enqueued () const;

	/** @return true if we are in the process of calling Encoder::process_end */
	bool finishing () const {
		return _finishing;
	}

private:

	void video (boost::shared_ptr<PlayerVideo>, DCPTime);
	void audio (boost::shared_ptr<AudioBuffers>, DCPTime);
	void subtitle (PlayerSubtitles, DCPTimePeriod);

	boost::shared_ptr<const Film> _film;
	boost::weak_ptr<Job> _job;
	boost::shared_ptr<Player> _player;
	boost::shared_ptr<Writer> _writer;
	boost::shared_ptr<Encoder> _encoder;
	bool _finishing;
	bool _non_burnt_subtitles;
};

/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "types.h"

/** @file  src/transcoder.h
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

class Film;
class Job;
class Encoder;
class VideoFilter;
class Player;

/** @class Transcoder
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */
class Transcoder
{
public:
	Transcoder (
		boost::shared_ptr<const Film> f,
		boost::shared_ptr<Job> j
		);

	void go ();

	float current_encoding_rate () const;
	int video_frames_out () const;

private:
	/** A Job that is running this Transcoder, or 0 */
	boost::shared_ptr<Job> _job;
	boost::shared_ptr<Player> _player;
	boost::shared_ptr<Encoder> _encoder;
};

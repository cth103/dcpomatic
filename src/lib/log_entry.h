/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_LOG_ENTRY_H
#define DCPOMATIC_LOG_ENTRY_H


#include <sys/time.h>
#include <string>


class LogEntry
{
public:

	static const int TYPE_GENERAL;
	static const int TYPE_WARNING;
	static const int TYPE_ERROR;
	static const int TYPE_DEBUG_THREE_D;
	static const int TYPE_DEBUG_ENCODE;
	static const int TYPE_TIMING;
	static const int TYPE_DEBUG_EMAIL;
	static const int TYPE_DEBUG_VIDEO_VIEW; ///< real-time video viewing (i.e. "playback")
	static const int TYPE_DISK;
	static const int TYPE_DEBUG_PLAYER;     ///< the Player class
	static const int TYPE_DEBUG_AUDIO_ANALYSIS; ///< audio analysis job

	explicit LogEntry (int type);
	virtual ~LogEntry () {}

	virtual std::string message () const = 0;

	int type () const {
		return _type;
	}
	std::string get () const;
	double seconds () const;

private:
	struct timeval _time;
	int _type;
};


#endif

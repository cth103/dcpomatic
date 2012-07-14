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

#ifndef DVDOMATIC_PLAYER_H
#define DVDOMATIC_PLAYER_H

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

class FilmState;
class Screen;

class Player
{
public:
	enum Split {
		SPLIT_NONE,
		SPLIT_LEFT,
		SPLIT_RIGHT
	};
		
	Player (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Screen>, Split);
	~Player ();

	void command (std::string);

	float position () const;
	bool paused () const;
	
	pid_t mplayer_pid () const {
		return _mplayer_pid;
	}

private:
	void stdout_reader ();
	void set_position (float);
	void set_paused (bool);
	
	int _mplayer_stdin[2];
	int _mplayer_stdout[2];
	int _mplayer_stderr[2];
	pid_t _mplayer_pid;

	boost::thread* _stdout_reader;
	/* XXX: should probably be atomically accessed */
	bool _stdout_reader_should_run;

	mutable boost::mutex _state_mutex;
	float _position;
	bool _paused;
};

#endif

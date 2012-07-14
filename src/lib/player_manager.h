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

#include <list>
#include <boost/shared_ptr.hpp>
#include "player.h"

class Player;
class FilmState;
class Screen;

class PlayerManager
{
public:

	void setup (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Screen>);
	void setup (boost::shared_ptr<const FilmState>, boost::shared_ptr<const FilmState>, boost::shared_ptr<const Screen>);
	void pause_or_unpause ();
	void stop ();

	float position () const;
	void set_position (float);

	enum State {
		QUIESCENT,
		PLAYING,
		PAUSED
	};

	State state () const;

	void child_exited (pid_t);

	static PlayerManager* instance ();

private:
	PlayerManager ();

	mutable boost::mutex _players_mutex;
	std::list<boost::shared_ptr<Player> > _players;
	
	static PlayerManager* _instance;
};

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "server.h"
#include <boost/signals2.hpp>

class ServerFinder : public ExceptionStore
{
public:
	void connect (boost::function<void (ServerDescription)>);

	static ServerFinder* instance ();

	void disable () {
		_disabled = true;
	}

private:
	ServerFinder ();

	void broadcast_thread ();
	void listen_thread ();

	bool server_found (std::string) const;

	boost::signals2::signal<void (ServerDescription)> ServerFound;

	bool _disabled;
	
	/** Thread to periodically issue broadcasts to find encoding servers */
	boost::thread* _broadcast_thread;
	/** Thread to listen to the responses from servers */
	boost::thread* _listen_thread;

	std::list<ServerDescription> _servers;
	mutable boost::mutex _mutex;

	static ServerFinder* _instance;
};

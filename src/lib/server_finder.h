/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
	boost::signals2::connection connect (boost::function<void (ServerDescription)>);

	static ServerFinder* instance ();
	static void drop ();

	void disable () {
		_disabled = true;
	}

	bool disabled () const {
		return _disabled;
	}

private:
	ServerFinder ();
	~ServerFinder ();

	void broadcast_thread ();
	void listen_thread ();

	bool server_found (std::string) const;
	void start_accept ();
	void handle_accept (boost::system::error_code ec, boost::shared_ptr<Socket> socket);

	boost::signals2::signal<void (ServerDescription)> ServerFound;

	bool _disabled;
	
	/** Thread to periodically issue broadcasts to find encoding servers */
	boost::thread* _broadcast_thread;
	/** Thread to listen to the responses from servers */
	boost::thread* _listen_thread;

	std::list<ServerDescription> _servers;
	mutable boost::mutex _mutex;

	boost::asio::io_service _listen_io_service;
	boost::shared_ptr<boost::asio::ip::tcp::acceptor> _listen_acceptor;
	bool _stop;

	static ServerFinder* _instance;
};

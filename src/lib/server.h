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

#ifndef DCPOMATIC_SERVER_H
#define DCPOMATIC_SERVER_H

/** @file src/server.h
 *  @brief Class to describe a server to which we can send
 *  encoding work, and a class to implement such a server.
 */

#include "log.h"
#include "exceptions.h"
#include <libxml++/libxml++.h>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/thread/condition.hpp>
#include <boost/optional.hpp>
#include <string>

class Socket;

namespace cxml {
	class Node;
}

/** @class ServerDescription
 *  @brief Class to describe a server to which we can send encoding work.
 */
class ServerDescription
{
public:
	ServerDescription ()
		: _host_name ("")
		, _threads (1)
	{}
	
	/** @param h Server host name or IP address in string form.
	 *  @param t Number of threads to use on the server.
	 */
	ServerDescription (std::string h, int t)
		: _host_name (h)
		, _threads (t)
	{}

	/* Default copy constructor is fine */
	
	/** @return server's host name or IP address in string form */
	std::string host_name () const {
		return _host_name;
	}

	/** @return number of threads to use on the server */
	int threads () const {
		return _threads;
	}

	void set_host_name (std::string n) {
		_host_name = n;
	}

	void set_threads (int t) {
		_threads = t;
	}

private:
	/** server's host name */
	std::string _host_name;
	/** number of threads to use on the server */
	int _threads;
};

class Server : public ExceptionStore, public boost::noncopyable
{
public:
	Server (boost::shared_ptr<Log> log, bool verbose);
	~Server ();

	void run (int num_threads);

private:
	void worker_thread ();
	int process (boost::shared_ptr<Socket> socket, struct timeval &, struct timeval &);
	void broadcast_thread ();
	void broadcast_received ();
	void start_accept ();
	void handle_accept (boost::shared_ptr<Socket>, boost::system::error_code const &);

	bool _terminate;

	std::vector<boost::thread *> _worker_threads;
	std::list<boost::shared_ptr<Socket> > _queue;
	boost::mutex _worker_mutex;
	boost::condition _full_condition;
	boost::condition _empty_condition;
	boost::shared_ptr<Log> _log;
	bool _verbose;

	boost::asio::io_service _io_service;
	boost::asio::ip::tcp::acceptor _acceptor;

	struct Broadcast {

		Broadcast ()
			: thread (0)
			, socket (0)
		{}
		
		boost::thread* thread;
		boost::asio::ip::udp::socket* socket;
		char buffer[64];
		boost::asio::ip::udp::endpoint send_endpoint;
		boost::asio::io_service io_service;
		
	} _broadcast;
};

#endif

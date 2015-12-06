/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_ENCODE_SERVER_H
#define DCPOMATIC_ENCODE_SERVER_H

/** @file src/encode_server.h
 *  @brief EncodeServer class.
 */

#include "server.h"
#include "exception_store.h"
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/thread/condition.hpp>
#include <string>

class Socket;
class Log;

/** @class EncodeServer
 *  @brief A class to run a server which can accept requests to perform JPEG2000
 *  encoding work.
 */
class EncodeServer : public Server, public ExceptionStore
{
public:
	EncodeServer (boost::shared_ptr<Log> log, bool verbose, int num_threads);
	~EncodeServer ();

	void run ();

private:
	void handle (boost::shared_ptr<Socket>);
	void worker_thread ();
	int process (boost::shared_ptr<Socket> socket, struct timeval &, struct timeval &);
	void broadcast_thread ();
	void broadcast_received ();

	std::vector<boost::thread *> _worker_threads;
	std::list<boost::shared_ptr<Socket> > _queue;
	boost::condition _full_condition;
	boost::condition _empty_condition;
	boost::shared_ptr<Log> _log;
	bool _verbose;
	int _num_threads;

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

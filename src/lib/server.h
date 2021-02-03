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


#ifndef DCPOMATIC_SERVER_H
#define DCPOMATIC_SERVER_H


#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/thread/condition.hpp>
#include <string>


class Socket;


class Server
{
public:
	explicit Server (int port, int timeout = 30);
	virtual ~Server ();

	Server (Server const&) = delete;
	Server& operator= (Server const&) = delete;

	virtual void run ();
	void stop ();

protected:
	boost::mutex _mutex;
	bool _terminate;

private:
	virtual void handle (std::shared_ptr<Socket> socket) = 0;

	void start_accept ();
	void handle_accept (std::shared_ptr<Socket>, boost::system::error_code const &);

	boost::asio::io_service _io_service;
	boost::asio::ip::tcp::acceptor _acceptor;
	int _timeout;
};


#endif

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

#include "server.h"
#include "dcpomatic_socket.h"

#include "i18n.h"

using boost::shared_ptr;

Server::Server (int port)
	: _terminate (false)
	, _acceptor (_io_service, boost::asio::ip::tcp::endpoint (boost::asio::ip::tcp::v4(), port))
{

}

Server::~Server ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_terminate = true;
	_io_service.stop ();
}

void
Server::run ()
{
	start_accept ();
	_io_service.run ();
}

void
Server::start_accept ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_terminate) {
			return;
		}
	}

	shared_ptr<Socket> socket (new Socket);
	_acceptor.async_accept (socket->socket (), boost::bind (&Server::handle_accept, this, socket, boost::asio::placeholders::error));
}

void
Server::handle_accept (shared_ptr<Socket> socket, boost::system::error_code const & error)
{
	if (error) {
		return;
	}

	handle (socket);
	start_accept ();
}

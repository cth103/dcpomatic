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

#include <libcxml/cxml.h>
#include "server_finder.h"
#include "exceptions.h"
#include "util.h"
#include "config.h"
#include "cross.h"
#include "ui_signaller.h"

using std::string;
using std::stringstream;
using boost::shared_ptr;
using boost::scoped_array;

ServerFinder::ServerFinder ()
	: _broadcast_thread (0)
	, _listen_thread (0)
	, _terminate (false)
{
	_broadcast_thread = new boost::thread (boost::bind (&ServerFinder::broadcast_thread, this));
	_listen_thread = new boost::thread (boost::bind (&ServerFinder::listen_thread, this));
}

ServerFinder::~ServerFinder ()
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
	}
	
	if (_broadcast_thread && _broadcast_thread->joinable ()) {
		_broadcast_thread->join ();
	}
	delete _broadcast_thread;

	if (_listen_thread && _listen_thread->joinable ()) {
		_listen_thread->join ();
	}
	delete _listen_thread;
}

void
ServerFinder::broadcast_thread ()
{
	boost::system::error_code error;
	boost::asio::io_service io_service;
	boost::asio::ip::udp::socket socket (io_service);
	socket.open (boost::asio::ip::udp::v4(), error);
	if (error) {
		throw NetworkError ("failed to set up broadcast socket");
	}

        socket.set_option (boost::asio::ip::udp::socket::reuse_address (true));
        socket.set_option (boost::asio::socket_base::broadcast (true));
	
        boost::asio::ip::udp::endpoint end_point (boost::asio::ip::address_v4::broadcast(), Config::instance()->server_port_base() + 1);            

	while (1) {
		boost::mutex::scoped_lock lm (_mutex);
		if (_terminate) {
			socket.close (error);
			return;
		}
		
		string data = DCPOMATIC_HELLO;
		socket.send_to (boost::asio::buffer (data.c_str(), data.size() + 1), end_point);

		lm.unlock ();
		dcpomatic_sleep (10);
	}
}

void
ServerFinder::listen_thread ()
{
	while (1) {
		{
			/* See if we need to stop */
			boost::mutex::scoped_lock lm (_mutex);
			if (_terminate) {
				return;
			}
		}

		shared_ptr<Socket> sock (new Socket (10));

		try {
			sock->accept (Config::instance()->server_port_base() + 1);
		} catch (std::exception& e) {
			continue;
		}

		uint32_t length = sock->read_uint32 ();
		scoped_array<char> buffer (new char[length]);
		sock->read (reinterpret_cast<uint8_t*> (buffer.get()), length);
		
		stringstream s (buffer.get());
		shared_ptr<cxml::Document> xml (new cxml::Document ("ServerAvailable"));
		xml->read_stream (s);

		ui_signaller->emit (boost::bind (boost::ref (ServerFound), ServerDescription (
							 sock->socket().remote_endpoint().address().to_string (),
							 xml->number_child<int> ("Threads")
							 )));
	}
}

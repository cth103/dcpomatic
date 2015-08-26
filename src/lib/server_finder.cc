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

#include "server_finder.h"
#include "exceptions.h"
#include "util.h"
#include "config.h"
#include "cross.h"
#include "server_description.h"
#include "dcpomatic_socket.h"
#include "raw_convert.h"
#include <libcxml/cxml.h>
#include <boost/lambda/lambda.hpp>

#include "i18n.h"

using std::string;
using std::list;
using std::vector;
using std::cout;
using boost::shared_ptr;
using boost::scoped_array;
using boost::weak_ptr;

ServerFinder* ServerFinder::_instance = 0;

ServerFinder::ServerFinder ()
	: _disabled (false)
	, _search_thread (0)
	, _listen_thread (0)
	, _stop (false)
{
	_search_thread = new boost::thread (boost::bind (&ServerFinder::search_thread, this));
	_listen_thread = new boost::thread (boost::bind (&ServerFinder::listen_thread, this));
	Config::instance()->Changed.connect (boost::bind (&ServerFinder::config_changed, this, _1));
}

ServerFinder::~ServerFinder ()
{
	_stop = true;

	_search_condition.notify_all ();
	_search_thread->join ();

	_listen_io_service.stop ();
	_listen_thread->join ();
}

void
ServerFinder::search_thread ()
try
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

	string const data = DCPOMATIC_HELLO;

	while (!_stop) {
		if (Config::instance()->use_any_servers ()) {
			/* Broadcast to look for servers */
			try {
				boost::asio::ip::udp::endpoint end_point (boost::asio::ip::address_v4::broadcast(), Config::instance()->server_port_base() + 1);
				socket.send_to (boost::asio::buffer (data.c_str(), data.size() + 1), end_point);
			} catch (...) {

			}
		}

		/* Query our `definite' servers (if there are any) */
		vector<string> servers = Config::instance()->servers ();
		for (vector<string>::const_iterator i = servers.begin(); i != servers.end(); ++i) {
			if (server_found (*i)) {
				/* Don't bother asking a server that we already know about */
				continue;
			}
			try {
				boost::asio::ip::udp::resolver resolver (io_service);
				boost::asio::ip::udp::resolver::query query (*i, raw_convert<string> (Config::instance()->server_port_base() + 1));
				boost::asio::ip::udp::endpoint end_point (*resolver.resolve (query));
				socket.send_to (boost::asio::buffer (data.c_str(), data.size() + 1), end_point);
			} catch (...) {

			}
		}

		boost::mutex::scoped_lock lm (_search_condition_mutex);
		_search_condition.timed_wait (lm, boost::get_system_time() + boost::posix_time::seconds (10));
	}
}
catch (...)
{
	store_current ();
}

void
ServerFinder::listen_thread ()
try {
	using namespace boost::asio::ip;

	try {
		_listen_acceptor.reset (new tcp::acceptor (_listen_io_service, tcp::endpoint (tcp::v4(), Config::instance()->server_port_base() + 1)));
	} catch (...) {
		boost::throw_exception (NetworkError (_("Could not listen for remote encode servers.  Perhaps another instance of DCP-o-matic is running.")));
	}

	start_accept ();
	_listen_io_service.run ();
}
catch (...)
{
	store_current ();
}

void
ServerFinder::start_accept ()
{
	shared_ptr<Socket> socket (new Socket ());
	_listen_acceptor->async_accept (
		socket->socket(),
		boost::bind (&ServerFinder::handle_accept, this, boost::asio::placeholders::error, socket)
		);
}

void
ServerFinder::handle_accept (boost::system::error_code ec, shared_ptr<Socket> socket)
{
	if (ec) {
		start_accept ();
		return;
	}

	uint32_t length;
	socket->read (reinterpret_cast<uint8_t*> (&length), sizeof (uint32_t));
	length = ntohl (length);

	scoped_array<char> buffer (new char[length]);
	socket->read (reinterpret_cast<uint8_t*> (buffer.get()), length);

	string s (buffer.get());
	shared_ptr<cxml::Document> xml (new cxml::Document ("ServerAvailable"));
	xml->read_string (s);

	string const ip = socket->socket().remote_endpoint().address().to_string ();
	if (!server_found (ip) && xml->optional_number_child<int>("Version").get_value_or (0) == SERVER_LINK_VERSION) {
		ServerDescription sd (ip, xml->number_child<int> ("Threads"));
		{
			boost::mutex::scoped_lock lm (_mutex);
			_servers.push_back (sd);
		}
		emit (boost::bind (boost::ref (ServersListChanged)));
	}

	start_accept ();
}

bool
ServerFinder::server_found (string ip) const
{
	boost::mutex::scoped_lock lm (_mutex);
	list<ServerDescription>::const_iterator i = _servers.begin();
	while (i != _servers.end() && i->host_name() != ip) {
		++i;
	}

	return i != _servers.end ();
}

ServerFinder*
ServerFinder::instance ()
{
	if (!_instance) {
		_instance = new ServerFinder ();
	}

	return _instance;
}

void
ServerFinder::drop ()
{
	delete _instance;
	_instance = 0;
}

list<ServerDescription>
ServerFinder::servers () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _servers;
}

void
ServerFinder::config_changed (Config::Property what)
{
	if (what == Config::USE_ANY_SERVERS || what == Config::SERVERS) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			_servers.clear ();
		}
		ServersListChanged ();
		search_now ();
	}
}

void
ServerFinder::search_now ()
{
	_search_condition.notify_all ();
}

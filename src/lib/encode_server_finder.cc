/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcpomatic_socket.h"
#include "encode_server_description.h"
#include "encode_server_finder.h"
#include "exceptions.h"
#include "util.h"
#include "variant.h"
#include <libcxml/cxml.h>
#include <fmt/format.h>
#include <boost/bind/placeholders.hpp>
#include <boost/lambda/lambda.hpp>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
using boost::scoped_array;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


EncodeServerFinder* EncodeServerFinder::_instance = 0;


EncodeServerFinder::EncodeServerFinder ()
	: _stop (false)
{
	Config::instance()->Changed.connect (boost::bind (&EncodeServerFinder::config_changed, this, _1));
}


void
EncodeServerFinder::start ()
{
	_search_thread = boost::thread (boost::bind(&EncodeServerFinder::search_thread, this));
	_listen_thread = boost::thread (boost::bind(&EncodeServerFinder::listen_thread, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_search_thread.native_handle(), "encode-server-search");
	pthread_setname_np (_listen_thread.native_handle(), "encode-server-listen");
#endif
}


EncodeServerFinder::~EncodeServerFinder ()
{
	stop ();
}


void
EncodeServerFinder::stop ()
{
	boost::this_thread::disable_interruption dis;

	_stop = true;

	_search_condition.notify_all ();
	try {
		_search_thread.join();
	} catch (...) {}

	_listen_io_context.stop();
	try {
		_listen_thread.join ();
	} catch (...) {}

	boost::mutex::scoped_lock lm (_servers_mutex);
	_servers.clear ();
}


void
EncodeServerFinder::search_thread ()
try
{
	start_of_thread ("EncodeServerFinder-search");

	boost::system::error_code error;
	dcpomatic::io_context io_context;
	boost::asio::ip::udp::socket socket(io_context);
	socket.open (boost::asio::ip::udp::v4(), error);
	if (error) {
		throw NetworkError ("failed to set up broadcast socket");
	}

        socket.set_option (boost::asio::ip::udp::socket::reuse_address(true));
        socket.set_option (boost::asio::socket_base::broadcast(true));

	string const data = DCPOMATIC_HELLO;
	int const interval = 10;

	while (!_stop) {
		if (Config::instance()->use_any_servers()) {
			/* Broadcast to look for servers */
			try {
				boost::asio::ip::udp::endpoint end_point (boost::asio::ip::address_v4::broadcast(), HELLO_PORT);
				socket.send_to (boost::asio::buffer(data.c_str(), data.size() + 1), end_point);
			} catch (...) {

			}
		}

		/* Query our `definite' servers (if there are any) */
		for (auto const& i: Config::instance()->servers()) {
			try {
				boost::asio::ip::udp::resolver resolver(io_context);
#ifdef DCPOMATIC_HAVE_BOOST_ASIO_IP_BASIC_RESOLVER_RESULTS
				boost::asio::connect(socket, resolver.resolve(i, fmt::to_string(HELLO_PORT)));
				socket.send(boost::asio::buffer(data.c_str(), data.size() + 1));
#else
				boost::asio::ip::udp::resolver::query query(i, fmt::to_string(HELLO_PORT));
				boost::asio::ip::udp::endpoint end_point(*resolver.resolve(query));
				socket.send_to(boost::asio::buffer(data.c_str(), data.size() + 1), end_point);
#endif
			} catch (...) {

			}
		}

		/* Discard servers that we haven't seen for a while */
		bool removed = false;
		{
			boost::mutex::scoped_lock lm (_servers_mutex);

			auto i = _servers.begin();
			while (i != _servers.end()) {
				if (i->last_seen_seconds() > 2 * interval) {
					auto j = i;
					++j;
					_servers.erase (i);
					i = j;
					removed = true;
				} else {
					++i;
				}
			}
		}

		if (removed) {
			emit (boost::bind(boost::ref(ServersListChanged)));
		}

		boost::mutex::scoped_lock lm (_search_condition_mutex);
		_search_condition.timed_wait (lm, boost::get_system_time() + boost::posix_time::seconds(interval));
	}
}
catch (...)
{
	store_current ();
}


void
EncodeServerFinder::listen_thread ()
try {
	start_of_thread ("EncodeServerFinder-listen");

	using namespace boost::asio::ip;

	try {
		_listen_acceptor.reset (
			new tcp::acceptor(_listen_io_context, tcp::endpoint(tcp::v4(), is_batch_converter ? BATCH_SERVER_PRESENCE_PORT : MAIN_SERVER_PRESENCE_PORT))
			);
	} catch (...) {
		boost::throw_exception(NetworkError(variant::insert_dcpomatic(_("Could not listen for remote encode servers.  Perhaps another instance of {} is running."))));
	}

	start_accept ();
	_listen_io_context.run();
}
catch (...)
{
	store_current ();
}


void
EncodeServerFinder::start_accept ()
{
	_accept_socket = make_shared<Socket>();

	_listen_acceptor->async_accept (
		_accept_socket->socket(),
		boost::bind(&EncodeServerFinder::handle_accept, this, boost::asio::placeholders::error)
		);
}


void
EncodeServerFinder::handle_accept (boost::system::error_code ec)
{
	if (ec) {
		start_accept ();
		return;
	}

	string server_available;

	try {
		uint32_t length;
		_accept_socket->read (reinterpret_cast<uint8_t*>(&length), sizeof(uint32_t));
		length = ntohl (length);

		if (length > 65536) {
			start_accept();
			return;
		}

		scoped_array<char> buffer(new char[length]);
		_accept_socket->read (reinterpret_cast<uint8_t*>(buffer.get()), length);
		server_available = buffer.get();
	} catch (NetworkError&) {
		/* Maybe the server went away; let's just try again */
		start_accept();
		return;
	}

	auto xml = make_shared<cxml::Document>("ServerAvailable");
	xml->read_string(server_available);

	auto const ip = _accept_socket->socket().remote_endpoint().address().to_string();
	bool changed = false;
	{
		boost::mutex::scoped_lock lm (_servers_mutex);
		auto i = _servers.begin();
		while (i != _servers.end() && i->host_name() != ip) {
			++i;
		}

		if (i != _servers.end()) {
			i->set_seen();
		} else {
			EncodeServerDescription sd (ip, xml->number_child<int>("Threads"), xml->optional_number_child<int>("Version").get_value_or(0));
			_servers.push_back (sd);
			changed = true;
		}
	}

	if (changed) {
		emit (boost::bind(boost::ref (ServersListChanged)));
	}

	start_accept ();
}


EncodeServerFinder*
EncodeServerFinder::instance ()
{
	if (!_instance) {
		_instance = new EncodeServerFinder ();
		_instance->start ();
	}

	return _instance;
}


void
EncodeServerFinder::drop ()
{
	delete _instance;
	_instance = nullptr;
}


list<EncodeServerDescription>
EncodeServerFinder::servers () const
{
	boost::mutex::scoped_lock lm (_servers_mutex);
	return _servers;
}


void
EncodeServerFinder::config_changed (Config::Property what)
{
	if (what == Config::USE_ANY_SERVERS || what == Config::SERVERS) {
		{
			boost::mutex::scoped_lock lm (_servers_mutex);
			_servers.clear ();
		}
		ServersListChanged ();
		_search_condition.notify_all ();
	}
}

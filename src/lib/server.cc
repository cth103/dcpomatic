/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file src/server.cc
 *  @brief Class to describe a server to which we can send
 *  encoding work, and a class to implement such a server.
 */

#include "server.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include "dcp_video.h"
#include "config.h"
#include "cross.h"
#include "player_video.h"
#include "encoded_data.h"
#include "safe_stringstream.h"
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>
#include <string>
#include <vector>
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL(...)    _log->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _log->log (__VA_ARGS__, Log::TYPE_GENERAL);
#define LOG_ERROR(...)      _log->log (String::compose (__VA_ARGS__), Log::TYPE_ERROR);
#define LOG_ERROR_NC(...)   _log->log (__VA_ARGS__, Log::TYPE_ERROR);

using std::string;
using std::multimap;
using std::vector;
using std::list;
using std::cout;
using std::cerr;
using std::setprecision;
using std::fixed;
using boost::shared_ptr;
using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::thread;
using boost::bind;
using boost::scoped_array;
using boost::optional;
using dcp::Size;
using dcp::raw_convert;

Server::Server (shared_ptr<Log> log, bool verbose)
	: _terminate (false)
	, _log (log)
	, _verbose (verbose)
	, _acceptor (_io_service, boost::asio::ip::tcp::endpoint (boost::asio::ip::tcp::v4(), Config::instance()->server_port_base()))
{

}

Server::~Server ()
{
	{
		boost::mutex::scoped_lock lm (_worker_mutex);
		_terminate = true;
		_empty_condition.notify_all ();
	}

	for (vector<boost::thread*>::iterator i = _worker_threads.begin(); i != _worker_threads.end(); ++i) {
		(*i)->join ();
		delete *i;
	}

	_io_service.stop ();

	_broadcast.io_service.stop ();
	_broadcast.thread->join ();
}

/** @param after_read Filled in with gettimeofday() after reading the input from the network.
 *  @param after_encode Filled in with gettimeofday() after encoding the image.
 */
int
Server::process (shared_ptr<Socket> socket, struct timeval& after_read, struct timeval& after_encode)
{
	uint32_t length = socket->read_uint32 ();
	scoped_array<char> buffer (new char[length]);
	socket->read (reinterpret_cast<uint8_t*> (buffer.get()), length);

	string s (buffer.get());
	shared_ptr<cxml::Document> xml (new cxml::Document ("EncodingRequest"));
	xml->read_string (s);
	if (xml->number_child<int> ("Version") != SERVER_LINK_VERSION) {
		cerr << "Mismatched server/client versions\n";
		LOG_ERROR_NC ("Mismatched server/client versions");
		return -1;
	}

	shared_ptr<PlayerVideo> pvf (new PlayerVideo (xml, socket));

	DCPVideo dcp_video_frame (pvf, xml, _log);

	gettimeofday (&after_read, 0);
	
	shared_ptr<EncodedData> encoded = dcp_video_frame.encode_locally (boost::bind (&Log::dcp_log, _log.get(), _1, _2));

	gettimeofday (&after_encode, 0);
	
	try {
		encoded->send (socket);
	} catch (std::exception& e) {
		cerr << "Send failed; frame " << dcp_video_frame.index() << "\n";
		LOG_ERROR ("Send failed; frame %1", dcp_video_frame.index());
		throw;
	}

	return dcp_video_frame.index ();
}

void
Server::worker_thread ()
{
	while (true) {
		boost::mutex::scoped_lock lock (_worker_mutex);
		while (_queue.empty () && !_terminate) {
			_empty_condition.wait (lock);
		}

		if (_terminate) {
			return;
		}

		shared_ptr<Socket> socket = _queue.front ();
		_queue.pop_front ();
		
		lock.unlock ();

		int frame = -1;
		string ip;

		struct timeval start;
		struct timeval after_read;
		struct timeval after_encode;
		struct timeval end;
		
		gettimeofday (&start, 0);
		
		try {
			frame = process (socket, after_read, after_encode);
			ip = socket->socket().remote_endpoint().address().to_string();
		} catch (std::exception& e) {
			cerr << "Error: " << e.what() << "\n";
			LOG_ERROR ("Error: %1", e.what());
		}

		gettimeofday (&end, 0);

		socket.reset ();
		
		lock.lock ();

		if (frame >= 0) {
			struct timeval end;
			gettimeofday (&end, 0);

			SafeStringStream message;
			message.precision (2);
			message << fixed
				<< "Encoded frame " << frame << " from " << ip << ": "
				<< "receive " << (seconds(after_read) - seconds(start)) << "s "
				<< "encode " << (seconds(after_encode) - seconds(after_read)) << "s "
				<< "send " << (seconds(end) - seconds(after_encode)) << "s.";
						   
			if (_verbose) {
				cout << message.str() << "\n";
			}

			LOG_GENERAL_NC (message.str ());
		}
		
		_full_condition.notify_all ();
	}
}

void
Server::run (int num_threads)
{
	LOG_GENERAL ("Server starting with %1 threads", num_threads);
	if (_verbose) {
		cout << "DCP-o-matic server starting with " << num_threads << " threads.\n";
	}
	
	for (int i = 0; i < num_threads; ++i) {
		_worker_threads.push_back (new thread (bind (&Server::worker_thread, this)));
	}

	_broadcast.thread = new thread (bind (&Server::broadcast_thread, this));
	
	start_accept ();
	_io_service.run ();
}

void
Server::broadcast_thread ()
try
{
	boost::asio::ip::address address = boost::asio::ip::address_v4::any ();
	boost::asio::ip::udp::endpoint listen_endpoint (address, Config::instance()->server_port_base() + 1);

	_broadcast.socket = new boost::asio::ip::udp::socket (_broadcast.io_service);
	_broadcast.socket->open (listen_endpoint.protocol ());
	_broadcast.socket->bind (listen_endpoint);

	_broadcast.socket->async_receive_from (
		boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
		_broadcast.send_endpoint,
		boost::bind (&Server::broadcast_received, this)
		);

	_broadcast.io_service.run ();
}
catch (...)
{
	store_current ();
}

void
Server::broadcast_received ()
{
	_broadcast.buffer[sizeof(_broadcast.buffer) - 1] = '\0';

	if (strcmp (_broadcast.buffer, DCPOMATIC_HELLO) == 0) {
		/* Reply to the client saying what we can do */
		xmlpp::Document doc;
		xmlpp::Element* root = doc.create_root_node ("ServerAvailable");
		root->add_child("Threads")->add_child_text (raw_convert<string> (_worker_threads.size ()));
		string xml = doc.write_to_string ("UTF-8");

		if (_verbose) {
			cout << "Offering services to master " << _broadcast.send_endpoint.address().to_string () << "\n";
		}
		shared_ptr<Socket> socket (new Socket);
		try {
			socket->connect (boost::asio::ip::tcp::endpoint (_broadcast.send_endpoint.address(), Config::instance()->server_port_base() + 1));
			socket->write (xml.length() + 1);
			socket->write ((uint8_t *) xml.c_str(), xml.length() + 1);
		} catch (...) {

		}
	}
		
	_broadcast.socket->async_receive_from (
		boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
		_broadcast.send_endpoint, boost::bind (&Server::broadcast_received, this)
		);
}

void
Server::start_accept ()
{
	if (_terminate) {
		return;
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

	boost::mutex::scoped_lock lock (_worker_mutex);
	
	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _worker_threads.size() * 2 && !_terminate) {
		_full_condition.wait (lock);
	}
	
	_queue.push_back (socket);
	_empty_condition.notify_all ();

	start_accept ();
}
	

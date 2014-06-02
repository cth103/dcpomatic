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

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include "server.h"
#include "util.h"
#include "scaler.h"
#include "image.h"
#include "dcp_video_frame.h"
#include "config.h"
#include "cross.h"
#include "player_video_frame.h"

#include "i18n.h"

#define LOG_GENERAL(...)    _log->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _log->log (__VA_ARGS__, Log::TYPE_GENERAL);
#define LOG_ERROR(...)      _log->log (String::compose (__VA_ARGS__), Log::TYPE_ERROR);
#define LOG_ERROR_NC(...)   _log->log (__VA_ARGS__, Log::TYPE_ERROR);

using std::string;
using std::stringstream;
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
	: _log (log)
	, _verbose (verbose)
{

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

	stringstream s (buffer.get());
	shared_ptr<cxml::Document> xml (new cxml::Document ("EncodingRequest"));
	xml->read_stream (s);
	if (xml->number_child<int> ("Version") != SERVER_LINK_VERSION) {
		cerr << "Mismatched server/client versions\n";
		LOG_ERROR_NC ("Mismatched server/client versions");
		return -1;
	}

	shared_ptr<PlayerVideoFrame> pvf (new PlayerVideoFrame (xml, socket, _log));

	DCPVideoFrame dcp_video_frame (pvf, xml, _log);

	gettimeofday (&after_read, 0);
	
	shared_ptr<EncodedData> encoded = dcp_video_frame.encode_locally ();

	gettimeofday (&after_encode, 0);
	
	try {
		encoded->send (socket);
	} catch (std::exception& e) {
		LOG_ERROR ("Send failed; frame %1", dcp_video_frame.index());
		throw;
	}

	return dcp_video_frame.index ();
}

void
Server::worker_thread ()
{
	while (1) {
		boost::mutex::scoped_lock lock (_worker_mutex);
		while (_queue.empty ()) {
			_worker_condition.wait (lock);
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
			LOG_ERROR ("Error: %1", e.what());
		}

		gettimeofday (&end, 0);

		socket.reset ();
		
		lock.lock ();

		if (frame >= 0) {
			struct timeval end;
			gettimeofday (&end, 0);

			stringstream message;
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
		
		_worker_condition.notify_all ();
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
	
	boost::asio::io_service io_service;

	boost::asio::ip::tcp::acceptor acceptor (
		io_service,
		boost::asio::ip::tcp::endpoint (boost::asio::ip::tcp::v4(), Config::instance()->server_port_base ())
		);
	
	while (1) {
		shared_ptr<Socket> socket (new Socket);
		acceptor.accept (socket->socket ());

		boost::mutex::scoped_lock lock (_worker_mutex);
		
		/* Wait until the queue has gone down a bit */
		while (int (_queue.size()) >= num_threads * 2) {
			_worker_condition.wait (lock);
		}
		
		_queue.push_back (socket);
		_worker_condition.notify_all ();
	}
}

void
Server::broadcast_thread ()
try
{
	boost::asio::io_service io_service;

	boost::asio::ip::address address = boost::asio::ip::address_v4::any ();
	boost::asio::ip::udp::endpoint listen_endpoint (address, Config::instance()->server_port_base() + 1);

	_broadcast.socket = new boost::asio::ip::udp::socket (io_service);
	_broadcast.socket->open (listen_endpoint.protocol ());
	_broadcast.socket->bind (listen_endpoint);

	_broadcast.socket->async_receive_from (
		boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
		_broadcast.send_endpoint,
		boost::bind (&Server::broadcast_received, this)
		);

	io_service.run ();
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
		stringstream xml;
		doc.write_to_stream (xml, "UTF-8");

		shared_ptr<Socket> socket (new Socket);
		try {
			socket->connect (boost::asio::ip::tcp::endpoint (_broadcast.send_endpoint.address(), Config::instance()->server_port_base() + 1));
			socket->write (xml.str().length() + 1);
			socket->write ((uint8_t *) xml.str().c_str(), xml.str().length() + 1);
		} catch (...) {

		}
	}
		
	_broadcast.socket->async_receive_from (
		boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
		_broadcast.send_endpoint, boost::bind (&Server::broadcast_received, this)
		);
}

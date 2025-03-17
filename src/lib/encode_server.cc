/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/encode_server.cc
 *  @brief Class to describe a server to which we can send
 *  encoding work, and a class to implement such a server.
 */


#include "compose.hpp"
#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "encode_server.h"
#include "encoded_log_entry.h"
#include "image.h"
#include "log.h"
#include "player_video.h"
#include "util.h"
#include "variant.h"
#include "version.h"
#include <dcp/warnings.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>
#ifdef HAVE_VALGRIND_H
#include <valgrind/memcheck.h>
#endif
#include <string>
#include <vector>
#include <iostream>

#include "i18n.h"


using std::string;
using std::vector;
using std::list;
using std::cout;
using std::cerr;
using std::fixed;
using std::shared_ptr;
using std::make_shared;
using boost::thread;
using boost::bind;
using boost::scoped_array;
using boost::optional;
using dcp::ArrayData;
using dcp::Size;


EncodeServer::EncodeServer (bool verbose, int num_threads)
#if !defined(RUNNING_ON_VALGRIND) || RUNNING_ON_VALGRIND == 0
	: Server (ENCODE_FRAME_PORT)
#else
	: Server (ENCODE_FRAME_PORT, 2400)
#endif
	, _verbose (verbose)
	, _num_threads (num_threads)
	, _frames_encoded(0)
{

}


EncodeServer::~EncodeServer ()
{
	boost::this_thread::disable_interruption dis;

	{
		boost::mutex::scoped_lock lm (_mutex);
		_terminate = true;
		_empty_condition.notify_all ();
		_full_condition.notify_all ();
	}

	try {
		_worker_threads.join_all ();
	} catch (...) {}

	{
		boost::mutex::scoped_lock lm (_broadcast.mutex);
		if (_broadcast.socket) {
			_broadcast.socket->close ();
			delete _broadcast.socket;
			_broadcast.socket = 0;
		}
	}

	_broadcast.io_context.stop();
	try {
		_broadcast.thread.join ();
	} catch (...) {}
}


/** @param after_read Filled in with gettimeofday() after reading the input from the network.
 *  @param after_encode Filled in with gettimeofday() after encoding the image.
 */
int
EncodeServer::process (shared_ptr<Socket> socket, struct timeval& after_read, struct timeval& after_encode)
{
	Socket::ReadDigestScope ds (socket);

	auto length = socket->read_uint32 ();
	if (length > 65536) {
		throw NetworkError("Malformed encode request (too large)");
	}

	scoped_array<char> buffer (new char[length]);
	socket->read (reinterpret_cast<uint8_t*>(buffer.get()), length);

	string s (buffer.get());
	auto xml = make_shared<cxml::Document>("EncodingRequest");
	xml->read_string (s);
	/* This is a double-check; the server shouldn't even be on the candidate list
	   if it is the wrong version, but it doesn't hurt to make sure here.
	*/
	if (xml->number_child<int> ("Version") != SERVER_LINK_VERSION) {
		cerr << "Mismatched server/client versions\n";
		LOG_ERROR_NC ("Mismatched server/client versions");
		return -1;
	}

	auto pvf = make_shared<PlayerVideo>(xml, socket);

	if (!ds.check()) {
		throw NetworkError ("Checksums do not match");
	}

	DCPVideo dcp_video_frame (pvf, xml);

	gettimeofday (&after_read, 0);

	auto encoded = dcp_video_frame.encode_locally ();

	gettimeofday (&after_encode, 0);

	try {
		Socket::WriteDigestScope ds (socket);
		socket->write (encoded.size());
		socket->write (encoded.data(), encoded.size());
	} catch (std::exception& e) {
		cerr << "Send failed; frame " << dcp_video_frame.index() << "\n";
		LOG_ERROR ("Send failed; frame %1", dcp_video_frame.index());
		throw;
	}

	++_frames_encoded;

	return dcp_video_frame.index ();
}


void
EncodeServer::worker_thread ()
{
	while (true) {
		boost::mutex::scoped_lock lock (_mutex);
		while (_queue.empty () && !_terminate) {
			_empty_condition.wait (lock);
		}

		if (_terminate) {
			return;
		}

		auto socket = _queue.front ();
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

			auto e = make_shared<EncodedLogEntry>(
				frame, ip,
				seconds(after_read) - seconds(start),
				seconds(after_encode) - seconds(after_read),
				seconds(end) - seconds(after_encode)
				);

			if (_verbose) {
				cout << e->get() << "\n";
			}

			dcpomatic_log->log (e);
		}

		_full_condition.notify_all ();
	}
}


void
EncodeServer::run ()
{
	LOG_GENERAL ("Server %1 (%2) starting with %3 threads", dcpomatic_version, dcpomatic_git_commit, _num_threads);
	if (_verbose) {
		cout << variant::dcpomatic_encode_server() << " starting with " << _num_threads << " threads.\n";
	}

	for (int i = 0; i < _num_threads; ++i) {
#ifdef DCPOMATIC_LINUX
		boost::thread* t = _worker_threads.create_thread (bind(&EncodeServer::worker_thread, this));
		pthread_setname_np (t->native_handle(), "encode-server-worker");
#else
		_worker_threads.create_thread (bind(&EncodeServer::worker_thread, this));
#endif
	}

	_broadcast.thread = thread (bind(&EncodeServer::broadcast_thread, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_broadcast.thread.native_handle(), "encode-server-broadcast");
#endif

	Server::run ();
}


void
EncodeServer::broadcast_thread ()
try
{
	boost::asio::ip::udp::endpoint listen_endpoint(boost::asio::ip::udp::v4(), HELLO_PORT);

	_broadcast.socket = new boost::asio::ip::udp::socket(_broadcast.io_context, listen_endpoint);

	_broadcast.socket->async_receive_from (
		boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
		_broadcast.send_endpoint,
		boost::bind (&EncodeServer::broadcast_received, this)
		);

	_broadcast.io_context.run();
}
catch (...)
{
	store_current ();
}


void
EncodeServer::broadcast_received ()
{
	_broadcast.buffer[sizeof(_broadcast.buffer) - 1] = '\0';

	if (strcmp (_broadcast.buffer, DCPOMATIC_HELLO) == 0) {
		/* Reply to the client saying what we can do */
		xmlpp::Document doc;
		auto root = doc.create_root_node ("ServerAvailable");
		cxml::add_text_child(root, "Threads", fmt::to_string(_worker_threads.size()));
		cxml::add_text_child(root, "Version", fmt::to_string(SERVER_LINK_VERSION));
		auto xml = doc.write_to_string ("UTF-8");

		if (_verbose) {
			cout << "Offering services to master " << _broadcast.send_endpoint.address().to_string () << "\n";
		}

		try {
			auto socket = make_shared<Socket>();
			socket->connect(_broadcast.send_endpoint.address(), MAIN_SERVER_PRESENCE_PORT);
			socket->write (xml.bytes() + 1);
			socket->write ((uint8_t *) xml.c_str(), xml.bytes() + 1);
		} catch (...) {

		}

		try {
			auto socket = make_shared<Socket>();
			socket->connect(_broadcast.send_endpoint.address(), BATCH_SERVER_PRESENCE_PORT);
			socket->write (xml.bytes() + 1);
			socket->write ((uint8_t *) xml.c_str(), xml.bytes() + 1);
		} catch (...) {

		}
	}

	boost::mutex::scoped_lock lm (_broadcast.mutex);
	if (_broadcast.socket) {
		_broadcast.socket->async_receive_from (
			boost::asio::buffer (_broadcast.buffer, sizeof (_broadcast.buffer)),
			_broadcast.send_endpoint, boost::bind (&EncodeServer::broadcast_received, this)
			);
	}
}


void
EncodeServer::handle (shared_ptr<Socket> socket)
{
	boost::mutex::scoped_lock lock (_mutex);

	_waker.nudge ();

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _worker_threads.size() * 2 && !_terminate) {
		_full_condition.wait (lock);
	}

	_queue.push_back (socket);
	_empty_condition.notify_all ();
}

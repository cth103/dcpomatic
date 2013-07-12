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

/** @file src/server.cc
 *  @brief Class to describe a server to which we can send
 *  encoding work, and a class to implement such a server.
 */

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>
#include <libcxml/cxml.h>
#include "server.h"
#include "util.h"
#include "scaler.h"
#include "image.h"
#include "dcp_video_frame.h"
#include "config.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using std::multimap;
using std::vector;
using boost::shared_ptr;
using boost::algorithm::is_any_of;
using boost::algorithm::split;
using boost::thread;
using boost::bind;
using boost::scoped_array;
using libdcp::Size;

ServerDescription::ServerDescription (shared_ptr<const cxml::Node> node)
{
	_host_name = node->string_child ("HostName");
	_threads = node->number_child<int> ("Threads");
}

void
ServerDescription::as_xml (xmlpp::Node* root) const
{
	root->add_child("HostName")->add_child_text (_host_name);
	root->add_child("Threads")->add_child_text (boost::lexical_cast<string> (_threads));
}

/** Create a server description from a string of metadata returned from as_metadata().
 *  @param v Metadata.
 *  @return ServerDescription, or 0.
 */
ServerDescription *
ServerDescription::create_from_metadata (string v)
{
	vector<string> b;
	split (b, v, is_any_of (N_(" ")));

	if (b.size() != 2) {
		return 0;
	}

	return new ServerDescription (b[0], atoi (b[1].c_str ()));
}

Server::Server (shared_ptr<Log> log)
	: _log (log)
{

}

int
Server::process (shared_ptr<Socket> socket)
{
	uint32_t length = socket->read_uint32 ();
	scoped_array<char> buffer (new char[length]);
	socket->read (reinterpret_cast<uint8_t*> (buffer.get()), length);
	
	stringstream s (buffer.get());
	multimap<string, string> kv = read_key_value (s);

	if (get_required_string (kv, "encode") != "please") {
		return -1;
	}

	libdcp::Size size (get_required_int (kv, "width"), get_required_int (kv, "height"));
	int frame = get_required_int (kv, "frame");
	int frames_per_second = get_required_int (kv, "frames_per_second");
	int j2k_bandwidth = get_required_int (kv, "j2k_bandwidth");

	shared_ptr<Image> image (new Image (PIX_FMT_RGB24, size, true));

	image->read_from_socket (socket);

	DCPVideoFrame dcp_video_frame (
		image, frame, frames_per_second, j2k_bandwidth, _log
		);
	
	shared_ptr<EncodedData> encoded = dcp_video_frame.encode_locally ();
	try {
		encoded->send (socket);
	} catch (std::exception& e) {
		_log->log (String::compose (
				   N_("Send failed; frame %1, data size %2, pixel format %3, image size %4x%5, %6 components"),
				   frame, encoded->size(), image->pixel_format(), image->size().width, image->size().height, image->components()
				   )
			);
		throw;
	}

	return frame;
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

		struct timeval start;
		gettimeofday (&start, 0);
		
		try {
			frame = process (socket);
		} catch (std::exception& e) {
			_log->log (String::compose (N_("Error: %1"), e.what()));
		}
		
		socket.reset ();
		
		lock.lock ();

		if (frame >= 0) {
			struct timeval end;
			gettimeofday (&end, 0);
			_log->log (String::compose (N_("Encoded frame %1 in %2"), frame, seconds (end) - seconds (start)));
		}
		
		_worker_condition.notify_all ();
	}
}

void
Server::run (int num_threads)
{
	_log->log (String::compose (N_("Server starting with %1 threads"), num_threads));
	
	for (int i = 0; i < num_threads; ++i) {
		_worker_threads.push_back (new thread (bind (&Server::worker_thread, this)));
	}

	boost::asio::io_service io_service;
	boost::asio::ip::tcp::acceptor acceptor (io_service, boost::asio::ip::tcp::endpoint (boost::asio::ip::tcp::v4(), Config::instance()->server_port ()));
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

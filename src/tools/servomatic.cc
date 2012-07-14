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

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include "config.h"
#include "dcp_video_frame.h"
#include "exceptions.h"
#include "util.h"
#include "config.h"
#include "scaler.h"
#include "image.h"
#include "log.h"

#define BACKLOG 8

using namespace std;
using namespace boost;

static vector<thread *> worker_threads;

static std::list<int> queue;
static mutex worker_mutex;
static condition worker_condition;
static Log log_ ("servomatic.log");

int
process (int fd)
{
	SocketReader reader (fd);
	
	char buffer[128];
	reader.read_indefinite ((uint8_t *) buffer, sizeof (buffer));
	reader.consume (strlen (buffer) + 1);
	
	stringstream s (buffer);
	
	string command;
	s >> command;
	if (command != "encode") {
		close (fd);
		return -1;
	}
	
	Size in_size;
	int pixel_format_int;
	Size out_size;
	int padding;
	string scaler_id;
	int frame;
	float frames_per_second;
	string post_process;
	int colour_lut_index;
	int j2k_bandwidth;
	
	s >> in_size.width >> in_size.height
	  >> pixel_format_int
	  >> out_size.width >> out_size.height
	  >> padding
	  >> scaler_id
	  >> frame
	  >> frames_per_second
	  >> post_process
	  >> colour_lut_index
	  >> j2k_bandwidth;
	
	PixelFormat pixel_format = (PixelFormat) pixel_format_int;
	Scaler const * scaler = Scaler::from_id (scaler_id);
	if (post_process == "none") {
		post_process = "";
	}
	
	shared_ptr<SimpleImage> image (new SimpleImage (pixel_format, in_size));
	
	for (int i = 0; i < image->components(); ++i) {
		int line_size;
		s >> line_size;
		image->set_line_size (i, line_size);
	}
	
	for (int i = 0; i < image->components(); ++i) {
		reader.read_definite_and_consume (image->data()[i], image->line_size()[i] * image->lines(i));
	}
	
#ifdef DEBUG_HASH
	image->hash ("Image for encoding (as received by server)");
#endif		
	
	DCPVideoFrame dcp_video_frame (image, out_size, padding, scaler, frame, frames_per_second, post_process, colour_lut_index, j2k_bandwidth, &log_);
	shared_ptr<EncodedData> encoded = dcp_video_frame.encode_locally ();
	encoded->send (fd);

#ifdef DEBUG_HASH
	encoded->hash ("Encoded image (as made by server and as sent back)");
#endif		

	
	return frame;
}

void
worker_thread ()
{
	while (1) {
		mutex::scoped_lock lock (worker_mutex);
		while (queue.empty ()) {
			worker_condition.wait (lock);
		}

		int fd = queue.front ();
		queue.pop_front ();
		
		lock.unlock ();

		int frame = -1;

		struct timeval start;
		gettimeofday (&start, 0);
		
		try {
			frame = process (fd);
		} catch (std::exception& e) {
			cerr << "Error: " << e.what() << "\n";
		}
		
		close (fd);
		
		lock.lock ();

		if (frame >= 0) {
			struct timeval end;
			gettimeofday (&end, 0);
			cout << "Encoded frame " << frame << " in " << (seconds (end) - seconds (start)) << "\n";
		}
		
		worker_condition.notify_all ();
	}
}

int
main ()
{
	Scaler::setup_scalers ();

	int const num_threads = Config::instance()->num_local_encoding_threads ();
	
	for (int i = 0; i < num_threads; ++i) {
		worker_threads.push_back (new thread (worker_thread));
	}
	
	int fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		throw NetworkError ("could not open socket");
	}

	int const o = 1;
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof (o));

	struct timeval tv;
	tv.tv_sec = 20;
	tv.tv_usec = 0;
	setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (void *) &tv, sizeof (tv));
	setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, (void *) &tv, sizeof (tv));

	struct sockaddr_in server_address;
	memset (&server_address, 0, sizeof (server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons (Config::instance()->server_port ());
	if (::bind (fd, (struct sockaddr *) &server_address, sizeof (server_address)) < 0) {
		stringstream s;
		s << "could not bind to port " << Config::instance()->server_port() << " (" << strerror (errno) << ")";
		throw NetworkError (s.str());
	}

	listen (fd, BACKLOG);

	while (1) {
		struct sockaddr_in client_address;
		socklen_t client_length = sizeof (client_address);
		int new_fd = accept (fd, (struct sockaddr *) &client_address, &client_length);
		if (new_fd < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				throw NetworkError ("accept failed");
			}

			continue;
		}

		mutex::scoped_lock lock (worker_mutex);
		
		/* Wait until the queue has gone down a bit */
		while (int (queue.size()) >= num_threads * 2) {
			worker_condition.wait (lock);
		}

		struct timeval tv;
		tv.tv_sec = 20;
		tv.tv_usec = 0;
		setsockopt (new_fd, SOL_SOCKET, SO_RCVTIMEO, (void *) &tv, sizeof (tv));
		setsockopt (new_fd, SOL_SOCKET, SO_SNDTIMEO, (void *) &tv, sizeof (tv));
		
		queue.push_back (new_fd);
		worker_condition.notify_all ();
	}
	
	close (fd);

	return 0;
}

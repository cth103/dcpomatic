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

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include <iostream>
#include <boost/lambda/lambda.hpp>
#include <libcxml/cxml.h>
#include "encoder.h"
#include "util.h"
#include "film.h"
#include "log.h"
#include "config.h"
#include "dcp_video_frame.h"
#include "server.h"
#include "cross.h"
#include "writer.h"

#include "i18n.h"

using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::list;
using std::cout;
using std::min;
using std::make_pair;
using boost::shared_ptr;
using boost::optional;
using boost::scoped_array;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding */
Encoder::Encoder (shared_ptr<const Film> f, shared_ptr<Job> j)
	: _film (f)
	, _job (j)
	, _video_frames_out (0)
	, _terminate (false)
	, _broadcast_thread (0)
	, _listen_thread (0)
{
	_have_a_real_frame[EYES_BOTH] = false;
	_have_a_real_frame[EYES_LEFT] = false;
	_have_a_real_frame[EYES_RIGHT] = false;
}

Encoder::~Encoder ()
{
	terminate_threads ();
	if (_writer) {
		_writer->finish ();
	}
}

/** Add a worker thread for a each thread on a remote server.  Caller must hold
 *  a lock on _mutex, or know that one is not currently required to
 *  safely modify _threads.
 */
void
Encoder::add_worker_threads (ServerDescription d)
{
	for (int i = 0; i < d.threads(); ++i) {
		_threads.push_back (
			make_pair (d, new boost::thread (boost::bind (&Encoder::encoder_thread, this, d)))
			);
	}
}

void
Encoder::process_begin ()
{
	for (int i = 0; i < Config::instance()->num_local_encoding_threads (); ++i) {
		_threads.push_back (
			make_pair (
				optional<ServerDescription> (),
				new boost::thread (boost::bind (&Encoder::encoder_thread, this, optional<ServerDescription> ()))
				)
			);
	}

	vector<ServerDescription> servers = Config::instance()->servers ();

	for (vector<ServerDescription>::iterator i = servers.begin(); i != servers.end(); ++i) {
		add_worker_threads (*i);
	}

	_broadcast_thread = new boost::thread (boost::bind (&Encoder::broadcast_thread, this));
	_listen_thread = new boost::thread (boost::bind (&Encoder::listen_thread, this));

	_writer.reset (new Writer (_film, _job));
}


void
Encoder::process_end ()
{
	boost::mutex::scoped_lock lock (_mutex);

	_film->log()->log (String::compose (N_("Clearing queue of %1"), _queue.size ()));

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		_film->log()->log (String::compose (N_("Waking with %1"), _queue.size ()), Log::VERBOSE);
		_condition.notify_all ();
		_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_threads ();

	_film->log()->log (String::compose (N_("Mopping up %1"), _queue.size()));

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideoFrame> >::iterator i = _queue.begin(); i != _queue.end(); ++i) {
		_film->log()->log (String::compose (N_("Encode left-over frame %1"), (*i)->frame ()));
		try {
			_writer->write ((*i)->encode_locally(), (*i)->frame (), (*i)->eyes ());
			frame_done ();
		} catch (std::exception& e) {
			_film->log()->log (String::compose (N_("Local encode failed (%1)"), e.what ()));
		}
	}
		
	_writer->finish ();
	_writer.reset ();
}	

/** @return an estimate of the current number of frames we are encoding per second,
 *  or 0 if not known.
 */
float
Encoder::current_encoding_rate () const
{
	boost::mutex::scoped_lock lock (_state_mutex);
	if (int (_time_history.size()) < _history_size) {
		return 0;
	}

	struct timeval now;
	gettimeofday (&now, 0);

	return _history_size / (seconds (now) - seconds (_time_history.back ()));
}

/** @return Number of video frames that have been sent out */
int
Encoder::video_frames_out () const
{
	boost::mutex::scoped_lock (_state_mutex);
	return _video_frames_out;
}

/** Should be called when a frame has been encoded successfully.
 *  @param n Source frame index.
 */
void
Encoder::frame_done ()
{
	boost::mutex::scoped_lock lock (_state_mutex);
	
	struct timeval tv;
	gettimeofday (&tv, 0);
	_time_history.push_front (tv);
	if (int (_time_history.size()) > _history_size) {
		_time_history.pop_back ();
	}
}

void
Encoder::process_video (shared_ptr<const Image> image, Eyes eyes, ColourConversion conversion, bool same)
{
	boost::mutex::scoped_lock lock (_mutex);

	/* XXX: discard 3D here if required */

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _threads.size() * 2 && !_terminate) {
		TIMING ("decoder sleeps with queue of %1", _queue.size());
		_condition.wait (lock);
		TIMING ("decoder wakes with queue of %1", _queue.size());
	}

	if (_terminate) {
		return;
	}

	if (_writer->thrown ()) {
		_writer->rethrow ();
	}

	if (_writer->can_fake_write (_video_frames_out)) {
		_writer->fake_write (_video_frames_out, eyes);
		_have_a_real_frame[eyes] = false;
		frame_done ();
	} else if (same && _have_a_real_frame[eyes]) {
		/* Use the last frame that we encoded. */
		_writer->repeat (_video_frames_out, eyes);
		frame_done ();
	} else {
		/* Queue this new frame for encoding */
		TIMING ("adding to queue of %1", _queue.size ());
		_queue.push_back (shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  image, _video_frames_out, eyes, conversion, _film->video_frame_rate(),
						  _film->j2k_bandwidth(), _film->log()
						  )
					  ));
		
		_condition.notify_all ();
		_have_a_real_frame[eyes] = true;
	}

	if (eyes != EYES_LEFT) {
		++_video_frames_out;
	}
}

void
Encoder::process_audio (shared_ptr<const AudioBuffers> data)
{
	_writer->write (data);
}

void
Encoder::terminate_threads ()
{
	{
		boost::mutex::scoped_lock lock (_mutex);
		_terminate = true;
		_condition.notify_all ();
	}

	for (ThreadList::iterator i = _threads.begin(); i != _threads.end(); ++i) {
		if (i->second->joinable ()) {
			i->second->join ();
		}
		delete i->second;
	}

	_threads.clear ();
		     
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
Encoder::encoder_thread (optional<ServerDescription> server)
{
	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;
	
	while (1) {

		TIMING ("encoder thread %1 sleeps", boost::this_thread::get_id());
		boost::mutex::scoped_lock lock (_mutex);
		while (_queue.empty () && !_terminate) {
			_condition.wait (lock);
		}

		if (_terminate) {
			return;
		}

		TIMING ("encoder thread %1 wakes with queue of %2", boost::this_thread::get_id(), _queue.size());
		shared_ptr<DCPVideoFrame> vf = _queue.front ();
		_film->log()->log (String::compose (N_("Encoder thread %1 pops frame %2 (%3) from queue"), boost::this_thread::get_id(), vf->frame(), vf->eyes ()));
		_queue.pop_front ();
		
		lock.unlock ();

		shared_ptr<EncodedData> encoded;

		if (server) {
			try {
				encoded = vf->encode_remotely (server.get ());

				if (remote_backoff > 0) {
					_film->log()->log (String::compose (N_("%1 was lost, but now she is found; removing backoff"), server->host_name ()));
				}
				
				/* This job succeeded, so remove any backoff */
				remote_backoff = 0;
				
			} catch (std::exception& e) {
				if (remote_backoff < 60) {
					/* back off more */
					remote_backoff += 10;
				}
				_film->log()->log (
					String::compose (
						N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
						vf->frame(), server->host_name(), e.what(), remote_backoff)
					);
			}
				
		} else {
			try {
				TIMING ("encoder thread %1 begins local encode of %2", boost::this_thread::get_id(), vf->frame());
				encoded = vf->encode_locally ();
				TIMING ("encoder thread %1 finishes local encode of %2", boost::this_thread::get_id(), vf->frame());
			} catch (std::exception& e) {
				_film->log()->log (String::compose (N_("Local encode failed (%1)"), e.what ()));
			}
		}

		if (encoded) {
			_writer->write (encoded, vf->frame (), vf->eyes ());
			frame_done ();
		} else {
			lock.lock ();
			_film->log()->log (
				String::compose (N_("Encoder thread %1 pushes frame %2 back onto queue after failure"), boost::this_thread::get_id(), vf->frame())
				);
			_queue.push_front (vf);
			lock.unlock ();
		}

		if (remote_backoff > 0) {
			dcpomatic_sleep (remote_backoff);
		}

		lock.lock ();
		_condition.notify_all ();
	}
}

void
Encoder::broadcast_thread ()
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
Encoder::listen_thread ()
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

		{
			/* See if we already know about this server */
			string const ip = sock->socket().remote_endpoint().address().to_string ();
			boost::mutex::scoped_lock lm (_mutex);
			ThreadList::iterator i = _threads.begin();
			while (i != _threads.end() && (!i->first || i->first->host_name() != ip)) {
				++i;
			}

			if (i == _threads.end ()) {
				add_worker_threads (ServerDescription (ip, xml->number_child<int> ("Threads")));
			}
		}
	}
}

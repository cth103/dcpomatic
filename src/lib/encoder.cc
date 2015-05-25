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

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include "encoder.h"
#include "util.h"
#include "film.h"
#include "log.h"
#include "config.h"
#include "dcp_video.h"
#include "server.h"
#include "cross.h"
#include "writer.h"
#include "server_finder.h"
#include "player.h"
#include "player_video.h"
#include <libcxml/cxml.h>
#include <boost/lambda/lambda.hpp>
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_ERROR);
#define LOG_TIMING(...) _film->log()->microsecond_log (String::compose (__VA_ARGS__), Log::TYPE_TIMING);

using std::pair;
using std::string;
using std::vector;
using std::list;
using std::cout;
using std::min;
using std::make_pair;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;
using boost::scoped_array;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding */
Encoder::Encoder (shared_ptr<const Film> f, weak_ptr<Job> j, shared_ptr<Writer> writer)
	: _film (f)
	, _job (j)
	, _video_frames_out (0)
	, _terminate (false)
	, _writer (writer)
{

}

Encoder::~Encoder ()
{
	terminate_threads ();
}

/** Add a worker thread for a each thread on a remote server.  Caller must hold
 *  a lock on _mutex, or know that one is not currently required to
 *  safely modify _threads.
 */
void
Encoder::add_worker_threads (ServerDescription d)
{
	LOG_GENERAL (N_("Adding %1 worker threads for remote %2"), d.threads(), d.host_name ());
	for (int i = 0; i < d.threads(); ++i) {
		_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, d)));
	}

	_writer->set_encoder_threads (_threads.size ());
}

void
Encoder::begin ()
{
	for (int i = 0; i < Config::instance()->num_local_encoding_threads (); ++i) {
		_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, optional<ServerDescription> ())));
	}

	_writer->set_encoder_threads (_threads.size ());

	if (!ServerFinder::instance()->disabled ()) {
		_server_found_connection = ServerFinder::instance()->connect (boost::bind (&Encoder::server_found, this, _1));
	}
}

void
Encoder::end ()
{
	boost::mutex::scoped_lock lock (_mutex);

	LOG_GENERAL (N_("Clearing queue of %1"), _queue.size ());

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_threads ();

	LOG_GENERAL (N_("Mopping up %1"), _queue.size());

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideo> >::iterator i = _queue.begin(); i != _queue.end(); ++i) {
		LOG_GENERAL (N_("Encode left-over frame %1"), (*i)->index ());
		try {
			_writer->write (
				(*i)->encode_locally (boost::bind (&Log::dcp_log, _film->log().get(), _1, _2)),
				(*i)->index (),
				(*i)->eyes ()
				);
			frame_done ();
		} catch (std::exception& e) {
			LOG_ERROR (N_("Local encode failed (%1)"), e.what ());
		}
	}
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

/** Called in order, so each time this is called the supplied frame is the one
 *  after the previous one.
 */
void
Encoder::enqueue (shared_ptr<PlayerVideo> pv)
{
	_waker.nudge ();
	
	boost::mutex::scoped_lock lock (_mutex);

	/* XXX: discard 3D here if required */

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _threads.size() * 2 && !_terminate) {
		LOG_TIMING ("decoder sleeps with queue of %1", _queue.size());
		_full_condition.wait (lock);
		LOG_TIMING ("decoder wakes with queue of %1", _queue.size());
	}

	if (_terminate) {
		return;
	}

	_writer->rethrow ();
	/* Re-throw any exception raised by one of our threads.  If more
	   than one has thrown an exception, only one will be rethrown, I think;
	   but then, if that happens something has gone badly wrong.
	*/
	rethrow ();

	if (_writer->can_fake_write (_video_frames_out)) {
		/* We can fake-write this frame */
		_writer->fake_write (_video_frames_out, pv->eyes ());
		frame_done ();
	} else if (pv->has_j2k ()) {
		/* This frame already has JPEG2000 data, so just write it */
		_writer->write (pv->j2k(), _video_frames_out, pv->eyes ());
	} else {
		/* Queue this new frame for encoding */
		LOG_TIMING ("adding to queue of %1", _queue.size ());
		_queue.push_back (shared_ptr<DCPVideo> (
					  new DCPVideo (
						  pv,
						  _video_frames_out,
						  _film->video_frame_rate(),
						  _film->j2k_bandwidth(),
						  _film->resolution(),
						  _film->burn_subtitles(),
						  _film->log()
						  )
					  ));

		/* The queue might not be empty any more, so notify anything which is
		   waiting on that.
		*/
		_empty_condition.notify_all ();
	}

	if (pv->eyes() != EYES_LEFT) {
		++_video_frames_out;
	}
}

void
Encoder::terminate_threads ()
{
	{
		boost::mutex::scoped_lock lock (_mutex);
		_terminate = true;
		_full_condition.notify_all ();
		_empty_condition.notify_all ();
	}

	for (list<boost::thread *>::iterator i = _threads.begin(); i != _threads.end(); ++i) {
		if ((*i)->joinable ()) {
			(*i)->join ();
		}
		delete *i;
	}

	_threads.clear ();
}

void
Encoder::encoder_thread (optional<ServerDescription> server)
try
{
	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;
	shared_ptr<DCPVideo> last_dcp_video;
	shared_ptr<EncodedData> last_encoded;
	
	while (true) {

		LOG_TIMING ("[%1] encoder thread sleeps", boost::this_thread::get_id());
		boost::mutex::scoped_lock lock (_mutex);
		while (_queue.empty () && !_terminate) {
			_empty_condition.wait (lock);
		}

		if (_terminate) {
			return;
		}

		LOG_TIMING ("[%1] encoder thread wakes with queue of %2", boost::this_thread::get_id(), _queue.size());
		shared_ptr<DCPVideo> vf = _queue.front ();
		LOG_TIMING ("[%1] encoder thread pops frame %2 (%3) from queue", boost::this_thread::get_id(), vf->index(), vf->eyes ());
		_queue.pop_front ();
		
		lock.unlock ();

		shared_ptr<EncodedData> encoded;

		if (last_dcp_video && vf->same (last_dcp_video)) {
			/* We already have encoded data for the same input as this one, so take a short-cut */
			encoded = last_encoded;
		} else {
			/* We need to encode this input */
			if (server) {
				try {
					encoded = vf->encode_remotely (server.get ());
					
					if (remote_backoff > 0) {
						LOG_GENERAL ("%1 was lost, but now she is found; removing backoff", server->host_name ());
					}
					
					/* This job succeeded, so remove any backoff */
					remote_backoff = 0;
					
				} catch (std::exception& e) {
					if (remote_backoff < 60) {
						/* back off more */
						remote_backoff += 10;
					}
					LOG_ERROR (
						N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
						vf->index(), server->host_name(), e.what(), remote_backoff
						);
				}
				
			} else {
				try {
					LOG_TIMING ("[%1] encoder thread begins local encode of %2", boost::this_thread::get_id(), vf->index());
					encoded = vf->encode_locally (boost::bind (&Log::dcp_log, _film->log().get(), _1, _2));
					LOG_TIMING ("[%1] encoder thread finishes local encode of %2", boost::this_thread::get_id(), vf->index());
				} catch (std::exception& e) {
					LOG_ERROR (N_("Local encode failed (%1)"), e.what ());
				}
			}
		}

		last_dcp_video = vf;
		last_encoded = encoded;

		if (encoded) {
			_writer->write (encoded, vf->index (), vf->eyes ());
			frame_done ();
		} else {
			lock.lock ();
			LOG_GENERAL (N_("[%1] Encoder thread pushes frame %2 back onto queue after failure"), boost::this_thread::get_id(), vf->index());
			_queue.push_front (vf);
			lock.unlock ();
		}

		if (remote_backoff > 0) {
			dcpomatic_sleep (remote_backoff);
		}

		/* The queue might not be full any more, so notify anything that is waiting on that */
		lock.lock ();
		_full_condition.notify_all ();
	}
}
catch (...)
{
	store_current ();
}

void
Encoder::server_found (ServerDescription s)
{
	add_worker_threads (s);
}

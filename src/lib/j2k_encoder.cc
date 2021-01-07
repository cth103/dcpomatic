/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

/** @file src/j2k_encoder.cc
 *  @brief J2K encoder class.
 */

#include "j2k_encoder.h"
#include "util.h"
#include "film.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "config.h"
#include "dcp_video.h"
#include "cross.h"
#include "writer.h"
#include "encode_server_finder.h"
#include "player.h"
#include "player_video.h"
#include "encode_server_description.h"
#include "compose.hpp"
#include <libcxml/cxml.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using std::exception;
using std::shared_ptr;
using std::weak_ptr;
using boost::optional;
using dcp::Data;
using namespace dcpomatic;

/** @param film Film that we are encoding.
 *  @param writer Writer that we are using.
 */
J2KEncoder::J2KEncoder (shared_ptr<const Film> film, shared_ptr<Writer> writer)
	: _film (film)
	, _history (200)
	, _writer (writer)
{
	servers_list_changed ();
}

J2KEncoder::~J2KEncoder ()
{
	boost::mutex::scoped_lock lm (_threads_mutex);
	terminate_threads ();
}

void
J2KEncoder::begin ()
{
	weak_ptr<J2KEncoder> wp = shared_from_this ();
	_server_found_connection = EncodeServerFinder::instance()->ServersListChanged.connect (
		boost::bind (&J2KEncoder::call_servers_list_changed, wp)
		);
}

/* We don't want the servers-list-changed callback trying to do things
   during destruction of J2KEncoder, and I think this is the neatest way
   to achieve that.
*/
void
J2KEncoder::call_servers_list_changed (weak_ptr<J2KEncoder> encoder)
{
	shared_ptr<J2KEncoder> e = encoder.lock ();
	if (e) {
		e->servers_list_changed ();
	}
}

void
J2KEncoder::end ()
{
	boost::mutex::scoped_lock lock (_queue_mutex);

	LOG_GENERAL (N_("Clearing queue of %1"), _queue.size ());

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		rethrow ();
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	lock.unlock ();

	LOG_GENERAL_NC (N_("Terminating encoder threads"));

	{
		boost::mutex::scoped_lock lm (_threads_mutex);
		terminate_threads ();
	}

	/* Something might have been thrown during terminate_threads */
	rethrow ();

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
				shared_ptr<dcp::Data>(new dcp::ArrayData((*i)->encode_locally())),
				(*i)->index(),
				(*i)->eyes()
				);
			frame_done ();
		} catch (std::exception& e) {
			LOG_ERROR (N_("Local encode failed (%1)"), e.what ());
		}
	}
}

/** @return an estimate of the current number of frames we are encoding per second,
 *  if known.
 */
optional<float>
J2KEncoder::current_encoding_rate () const
{
	return _history.rate ();
}

/** @return Number of video frames that have been queued for encoding */
int
J2KEncoder::video_frames_enqueued () const
{
	if (!_last_player_video_time) {
		return 0;
	}

	return _last_player_video_time->frames_floor (_film->video_frame_rate ());
}

/** Should be called when a frame has been encoded successfully */
void
J2KEncoder::frame_done ()
{
	_history.event ();
}

/** Called to request encoding of the next video frame in the DCP.  This is called in order,
 *  so each time the supplied frame is the one after the previous one.
 *  pv represents one video frame, and could be empty if there is nothing to encode
 *  for this DCP frame.
 *
 *  @param pv PlayerVideo to encode.
 *  @param time Time of \p pv within the DCP.
 */
void
J2KEncoder::encode (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	_waker.nudge ();

	size_t threads = 0;
	{
		boost::mutex::scoped_lock lm (_threads_mutex);
		threads = _threads->size();
	}

	boost::mutex::scoped_lock queue_lock (_queue_mutex);

	/* Wait until the queue has gone down a bit.  Allow one thing in the queue even
	   when there are no threads.
	*/
	while (_queue.size() >= (threads * 2) + 1) {
		LOG_TIMING ("decoder-sleep queue=%1 threads=%2", _queue.size(), threads);
		_full_condition.wait (queue_lock);
		LOG_TIMING ("decoder-wake queue=%1 threads=%2", _queue.size(), threads);
	}

	_writer->rethrow ();
	/* Re-throw any exception raised by one of our threads.  If more
	   than one has thrown an exception, only one will be rethrown, I think;
	   but then, if that happens something has gone badly wrong.
	*/
	rethrow ();

	Frame const position = time.frames_floor(_film->video_frame_rate());

	if (_writer->can_fake_write (position)) {
		/* We can fake-write this frame */
		LOG_DEBUG_ENCODE("Frame @ %1 FAKE", to_string(time));
		_writer->fake_write (position, pv->eyes ());
		frame_done ();
	} else if (pv->has_j2k() && !_film->reencode_j2k()) {
		LOG_DEBUG_ENCODE("Frame @ %1 J2K", to_string(time));
		/* This frame already has J2K data, so just write it */
		_writer->write (pv->j2k(), position, pv->eyes ());
		frame_done ();
	} else if (_last_player_video[pv->eyes()] && _writer->can_repeat(position) && pv->same (_last_player_video[pv->eyes()])) {
		LOG_DEBUG_ENCODE("Frame @ %1 REPEAT", to_string(time));
		_writer->repeat (position, pv->eyes ());
	} else {
		LOG_DEBUG_ENCODE("Frame @ %1 ENCODE", to_string(time));
		/* Queue this new frame for encoding */
		LOG_TIMING ("add-frame-to-queue queue=%1", _queue.size ());
		_queue.push_back (shared_ptr<DCPVideo> (
					  new DCPVideo (
						  pv,
						  position,
						  _film->video_frame_rate(),
						  _film->j2k_bandwidth(),
						  _film->resolution()
						  )
					  ));

		/* The queue might not be empty any more, so notify anything which is
		   waiting on that.
		*/
		_empty_condition.notify_all ();
	}

	_last_player_video[pv->eyes()] = pv;
	_last_player_video_time = time;
}


/** Caller must hold a lock on _threads_mutex */
void
J2KEncoder::terminate_threads ()
{
	boost::this_thread::disable_interruption dis;

	if (!_threads) {
		return;
	}

	_threads->interrupt_all ();
	try {
		_threads->join_all ();
	} catch (exception& e) {
		LOG_ERROR ("join() threw an exception: %1", e.what());
	} catch (...) {
		LOG_ERROR_NC ("join() threw an exception");
	}

	_threads.reset ();
}

void
J2KEncoder::encoder_thread (optional<EncodeServerDescription> server)
try
{
	if (server) {
		LOG_TIMING ("start-encoder-thread thread=%1 server=%2", thread_id (), server->host_name ());
	} else {
		LOG_TIMING ("start-encoder-thread thread=%1 server=localhost", thread_id ());
	}

	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;

	while (true) {

		LOG_TIMING ("encoder-sleep thread=%1", thread_id ());
		boost::mutex::scoped_lock lock (_queue_mutex);
		while (_queue.empty ()) {
			_empty_condition.wait (lock);
		}

		LOG_TIMING ("encoder-wake thread=%1 queue=%2", thread_id(), _queue.size());
		shared_ptr<DCPVideo> vf = _queue.front ();

		/* We're about to commit to either encoding this frame or putting it back onto the queue,
		   so we must not be interrupted until one or other of these things have happened.  This
		   block has thread interruption disabled.
		*/
		{
			boost::this_thread::disable_interruption dis;

			LOG_TIMING ("encoder-pop thread=%1 frame=%2 eyes=%3", thread_id(), vf->index(), (int) vf->eyes ());
			_queue.pop_front ();

			lock.unlock ();

			shared_ptr<Data> encoded;

			/* We need to encode this input */
			if (server) {
				try {
					encoded.reset(new dcp::ArrayData(vf->encode_remotely(server.get())));

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
					LOG_TIMING ("start-local-encode thread=%1 frame=%2", thread_id(), vf->index());
					encoded.reset(new dcp::ArrayData(vf->encode_locally()));
					LOG_TIMING ("finish-local-encode thread=%1 frame=%2", thread_id(), vf->index());
				} catch (std::exception& e) {
					/* This is very bad, so don't cope with it, just pass it on */
					LOG_ERROR (N_("Local encode failed (%1)"), e.what ());
					throw;
				}
			}

			if (encoded) {
				_writer->write (encoded, vf->index(), vf->eyes());
				frame_done ();
			} else {
				lock.lock ();
				LOG_GENERAL (N_("[%1] J2KEncoder thread pushes frame %2 back onto queue after failure"), thread_id(), vf->index());
				_queue.push_front (vf);
				lock.unlock ();
			}
		}

		if (remote_backoff > 0) {
			boost::this_thread::sleep (boost::posix_time::seconds (remote_backoff));
		}

		/* The queue might not be full any more, so notify anything that is waiting on that */
		lock.lock ();
		_full_condition.notify_all ();
	}
}
catch (boost::thread_interrupted& e) {
	/* Ignore these and just stop the thread */
	_full_condition.notify_all ();
}
catch (...)
{
	store_current ();
	/* Wake anything waiting on _full_condition so it can see the exception */
	_full_condition.notify_all ();
}

void
J2KEncoder::servers_list_changed ()
{
	boost::mutex::scoped_lock lm (_threads_mutex);

	terminate_threads ();
	_threads.reset (new boost::thread_group());

	/* XXX: could re-use threads */

	if (!Config::instance()->only_servers_encode ()) {
		for (int i = 0; i < Config::instance()->master_encoding_threads (); ++i) {
#ifdef DCPOMATIC_LINUX
			boost::thread* t = _threads->create_thread(boost::bind(&J2KEncoder::encoder_thread, this, optional<EncodeServerDescription>()));
			pthread_setname_np (t->native_handle(), "encode-worker");
#else
			_threads->create_thread(boost::bind(&J2KEncoder::encoder_thread, this, optional<EncodeServerDescription>()));
#endif
		}
	}

	BOOST_FOREACH (EncodeServerDescription i, EncodeServerFinder::instance()->servers()) {
		if (!i.current_link_version()) {
			continue;
		}

		LOG_GENERAL (N_("Adding %1 worker threads for remote %2"), i.threads(), i.host_name ());
		for (int j = 0; j < i.threads(); ++j) {
			_threads->create_thread(boost::bind(&J2KEncoder::encoder_thread, this, i));
		}
	}

	_writer->set_encoder_threads (_threads->size());
}

/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include "encoder.h"
#include "util.h"
#include "film.h"
#include "log.h"
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

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_GENERAL);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_ERROR);
#define LOG_TIMING(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_TIMING);

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;
using dcp::Data;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding */
Encoder::Encoder (shared_ptr<const Film> film, shared_ptr<Writer> writer)
	: _film (film)
	, _writer (writer)
{
	servers_list_changed ();
}

Encoder::~Encoder ()
{
	try {
		terminate_threads ();
	} catch (...) {
		/* Destructors must not throw exceptions; anything bad
		   happening now is too late to worry about anyway,
		   I think.
		*/
	}
}

void
Encoder::begin ()
{
	if (!EncodeServerFinder::instance()->disabled ()) {
		weak_ptr<Encoder> wp = shared_from_this ();
		_server_found_connection = EncodeServerFinder::instance()->ServersListChanged.connect (
			boost::bind (&Encoder::call_servers_list_changed, wp)
			);
	}
}

/* We don't want the servers-list-changed callback trying to do things
   during destruction of Encoder, and I think this is the neatest way
   to achieve that.
*/
void
Encoder::call_servers_list_changed (weak_ptr<Encoder> encoder)
{
	shared_ptr<Encoder> e = encoder.lock ();
	if (e) {
		e->servers_list_changed ();
	}
}

void
Encoder::end ()
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

/** @return Number of video frames that have been queued for encoding */
int
Encoder::video_frames_enqueued () const
{
	return _last_player_video->time().frames_floor (_film->video_frame_rate ());
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

/** Called to start encoding of the next video frame in the DCP.  This is called in order,
 *  so each time the supplied frame is the one after the previous one.
 *  pv represents one video frame, and could be empty if there is nothing to encode
 *  for this DCP frame.
 */
void
Encoder::encode (shared_ptr<PlayerVideo> pv)
{
	_waker.nudge ();

	size_t threads = 0;
	{
		boost::mutex::scoped_lock threads_lock (_threads_mutex);
		threads = _threads.size ();
	}

	boost::mutex::scoped_lock queue_lock (_queue_mutex);

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= threads * 2) {
		LOG_TIMING ("decoder-sleep queue=%1", _queue.size());
		_full_condition.wait (queue_lock);
		LOG_TIMING ("decoder-wake queue=%1", _queue.size());
	}

	_writer->rethrow ();
	/* Re-throw any exception raised by one of our threads.  If more
	   than one has thrown an exception, only one will be rethrown, I think;
	   but then, if that happens something has gone badly wrong.
	*/
	rethrow ();

	Frame const position = pv->time().frames_floor(_film->video_frame_rate());

	if (_writer->can_fake_write (position)) {
		/* We can fake-write this frame */
		_writer->fake_write (position, pv->eyes ());
		frame_done ();
	} else if (pv->has_j2k ()) {
		/* This frame already has JPEG2000 data, so just write it */
		_writer->write (pv->j2k(), position, pv->eyes ());
	} else if (_last_player_video && _writer->can_repeat(position) && pv->same (_last_player_video)) {
		_writer->repeat (position, pv->eyes ());
	} else {
		/* Queue this new frame for encoding */
		LOG_TIMING ("add-frame-to-queue queue=%1", _queue.size ());
		_queue.push_back (shared_ptr<DCPVideo> (
					  new DCPVideo (
						  pv,
						  position,
						  _film->video_frame_rate(),
						  _film->j2k_bandwidth(),
						  _film->resolution(),
						  _film->log()
						  )
					  ));

		/* The queue might not be empty any more, so notify anything which is
		   waiting on that.
		*/
		_empty_condition.notify_all ();
	}

	_last_player_video = pv;
}

void
Encoder::terminate_threads ()
{
	boost::mutex::scoped_lock threads_lock (_threads_mutex);

	int n = 0;
	for (list<boost::thread *>::iterator i = _threads.begin(); i != _threads.end(); ++i) {
		LOG_GENERAL ("Terminating thread %1 of %2", n + 1, _threads.size ());
		(*i)->interrupt ();
		DCPOMATIC_ASSERT ((*i)->joinable ());
		try {
			(*i)->join ();
		} catch (boost::thread_interrupted& e) {
			/* This is to be expected */
		}
		delete *i;
		LOG_GENERAL_NC ("Thread terminated");
		++n;
	}

	_threads.clear ();
}

void
Encoder::encoder_thread (optional<EncodeServerDescription> server)
try
{
	if (server) {
		LOG_TIMING ("start-encoder-thread thread=%1 server=%2", boost::this_thread::get_id (), server->host_name ());
	} else {
		LOG_TIMING ("start-encoder-thread thread=%1 server=localhost", boost::this_thread::get_id ());
	}

	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;

	while (true) {

		LOG_TIMING ("encoder-sleep thread=%1", boost::this_thread::get_id());
		boost::mutex::scoped_lock lock (_queue_mutex);
		while (_queue.empty ()) {
			_empty_condition.wait (lock);
		}

		LOG_TIMING ("encoder-wake thread=%1 queue=%2", boost::this_thread::get_id(), _queue.size());
		shared_ptr<DCPVideo> vf = _queue.front ();
		LOG_TIMING ("encoder-pop thread=%1 frame=%2 eyes=%3", boost::this_thread::get_id(), vf->index(), vf->eyes ());
		_queue.pop_front ();

		lock.unlock ();

		optional<Data> encoded;

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
				LOG_TIMING ("start-local-encode thread=%1 frame=%2", boost::this_thread::get_id(), vf->index());
				encoded = vf->encode_locally (boost::bind (&Log::dcp_log, _film->log().get(), _1, _2));
				LOG_TIMING ("finish-local-encode thread=%1 frame=%2", boost::this_thread::get_id(), vf->index());
			} catch (std::exception& e) {
				LOG_ERROR (N_("Local encode failed (%1)"), e.what ());
				throw;
			}
		}

		if (encoded) {
			_writer->write (encoded.get(), vf->index (), vf->eyes ());
			frame_done ();
		} else {
			lock.lock ();
			LOG_GENERAL (N_("[%1] Encoder thread pushes frame %2 back onto queue after failure"), boost::this_thread::get_id(), vf->index());
			_queue.push_front (vf);
			lock.unlock ();
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
Encoder::servers_list_changed ()
{
	terminate_threads ();

	/* XXX: could re-use threads */

	boost::mutex::scoped_lock lm (_threads_mutex);

#ifdef BOOST_THREAD_PLATFORM_WIN32
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	GetVersionEx (&info);
	bool const windows_xp = (info.dwMajorVersion == 5 && info.dwMinorVersion == 1);
	if (windows_xp) {
		LOG_GENERAL_NC (N_("Setting thread affinity for Windows XP"));
	}
#endif

	if (!Config::instance()->only_servers_encode ()) {
		for (int i = 0; i < Config::instance()->num_local_encoding_threads (); ++i) {
			boost::thread* t = new boost::thread (boost::bind (&Encoder::encoder_thread, this, optional<EncodeServerDescription> ()));
			_threads.push_back (t);
#ifdef BOOST_THREAD_PLATFORM_WIN32
			if (windows_xp) {
				SetThreadAffinityMask (t->native_handle(), 1 << i);
			}
#endif
		}
	}

	BOOST_FOREACH (EncodeServerDescription i, EncodeServerFinder::instance()->servers ()) {
		LOG_GENERAL (N_("Adding %1 worker threads for remote %2"), i.threads(), i.host_name ());
		for (int j = 0; j < i.threads(); ++j) {
			_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, i)));
		}
	}

	_writer->set_encoder_threads (_threads.size ());
}

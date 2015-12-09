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

#ifndef DCPOMATIC_ENCODER_H
#define DCPOMATIC_ENCODER_H

/** @file  src/encoder.h
 *  @brief Encoder class.
 */

#include "util.h"
#include "cross.h"
#include "exception_store.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <list>
#include <stdint.h>

class Film;
class ServerDescription;
class DCPVideo;
class Writer;
class Job;
class PlayerVideo;

/** @class Encoder
 *  @brief Class to manage encoding to JPEG2000.
 *
 *  This class keeps a queue of frames to be encoded and distributes
 *  the work around threads and encoding servers.
 */

class Encoder : public boost::noncopyable, public ExceptionStore
{
public:
	Encoder (boost::shared_ptr<const Film>, boost::shared_ptr<Writer>);
	virtual ~Encoder ();

	/** Called to indicate that a processing run is about to begin */
	void begin ();

	/** Called to pass in zero or more bits of video to be encoded
	 *  as the next DCP frame.
	 */
	void encode (std::list<boost::shared_ptr<PlayerVideo> > f);

	/** Called when a processing run has finished */
	void end ();

	float current_encoding_rate () const;
	int video_frames_out () const;

private:

	void enqueue (boost::shared_ptr<PlayerVideo> f);
	void frame_done ();

	void encoder_thread (boost::optional<ServerDescription>);
	void terminate_threads ();
	void servers_list_changed ();

	/** Film that we are encoding */
	boost::shared_ptr<const Film> _film;

	/** Mutex for _time_history and _video_frames_enqueued */
	mutable boost::mutex _state_mutex;
	/** List of the times of completion of the last _history_size frames;
	    first is the most recently completed.
	*/
	std::list<struct timeval> _time_history;
	/** Number of frames that we should keep history for */
	static int const _history_size;
	/** Current DCP frame index */
	Frame _position;

	/* XXX: probably should be atomic */
	bool _terminate_enqueue;
	bool _terminate_encoding;
	/** Mutex for _threads */
	mutable boost::mutex _threads_mutex;
	std::list<boost::thread *> _threads;
	mutable boost::mutex _queue_mutex;
	std::list<boost::shared_ptr<DCPVideo> > _queue;
	/** condition to manage thread wakeups when we have nothing to do */
	boost::condition _empty_condition;
	/** condition to manage thread wakeups when we have too much to do */
	boost::condition _full_condition;

	boost::shared_ptr<Writer> _writer;
	Waker _waker;

	boost::shared_ptr<PlayerVideo> _last_player_video;

	boost::signals2::scoped_connection _server_found_connection;
};

#endif

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

#ifndef DCPOMATIC_ENCODER_H
#define DCPOMATIC_ENCODER_H

/** @file src/encoder.h
 *  @brief Encoder to J2K and WAV for DCP.
 */

#include "util.h"
#include "config.h"
#include "cross.h"
#include "exceptions.h"
extern "C" {
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <list>
#include <stdint.h>

class Image;
class AudioBuffers;
class Film;
class ServerDescription;
class DCPVideo;
class EncodedData;
class Writer;
class Job;
class ServerFinder;
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
	Encoder (boost::shared_ptr<const Film> f, boost::weak_ptr<Job>, boost::shared_ptr<Writer>);
	virtual ~Encoder ();

	/** Called to indicate that a processing run is about to begin */
	void begin ();

	/** Call with a frame of video.
	 *  @param f Video frame.
	 */
	void enqueue (boost::shared_ptr<PlayerVideo> f);

	/** Called when a processing run has finished */
	void end ();

	float current_encoding_rate () const;
	int video_frames_out () const;

private:
	
	void frame_done ();
	
	void encoder_thread (boost::optional<ServerDescription>);
	void terminate_threads ();
	void add_worker_threads (ServerDescription);
	void server_found (ServerDescription);

	/** Film that we are encoding */
	boost::shared_ptr<const Film> _film;
	boost::weak_ptr<Job> _job;

	/** Mutex for _time_history and _last_frame */
	mutable boost::mutex _state_mutex;
	/** List of the times of completion of the last _history_size frames;
	    first is the most recently completed.
	*/
	std::list<struct timeval> _time_history;
	/** Number of frames that we should keep history for */
	static int const _history_size;

	/** Number of video frames written for the DCP so far */
	int _video_frames_out;

	bool _terminate;
	std::list<boost::shared_ptr<DCPVideo> > _queue;
	std::list<boost::thread *> _threads;
	mutable boost::mutex _mutex;
	/** condition to manage thread wakeups when we have nothing to do */
	boost::condition _empty_condition;
	/** condition to manage thread wakeups when we have too much to do */
	boost::condition _full_condition;

	boost::shared_ptr<Writer> _writer;
	Waker _waker;
};

#endif

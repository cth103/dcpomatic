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

#ifndef DCPOMATIC_ENCODER_H
#define DCPOMATIC_ENCODER_H

/** @file src/encoder.h
 *  @brief Encoder to J2K and WAV for DCP.
 */

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>
#include <boost/optional.hpp>
#include <list>
#include <stdint.h>
extern "C" {
#include <libavutil/samplefmt.h>
}
#ifdef HAVE_SWRESAMPLE
extern "C" {
#include <libswresample/swresample.h>
}
#endif
#include "util.h"
#include "video_sink.h"
#include "audio_sink.h"

class Image;
class Subtitle;
class AudioBuffers;
class Film;
class ServerDescription;
class DCPVideoFrame;
class EncodedData;
class Writer;
class Job;

/** @class Encoder
 *  @brief Encoder to J2K and WAV for DCP.
 *
 *  Video is supplied to process_video as YUV frames, and audio
 *  is supplied as uncompressed PCM in blocks of various sizes.
 */

class Encoder : public VideoSink, public AudioSink
{
public:
	Encoder (boost::shared_ptr<Film> f, boost::shared_ptr<Job>);
	virtual ~Encoder ();

	/** Called to indicate that a processing run is about to begin */
	void process_begin ();

	/** Call with a frame of video.
	 *  @param i Video frame image.
	 *  @param same true if i is the same as the last time we were called.
	 *  @param s A subtitle that should be on this frame, or 0.
	 */
	void process_video (boost::shared_ptr<const Image> i, bool same, boost::shared_ptr<Subtitle> s);

	/** Call with some audio data */
	void process_audio (boost::shared_ptr<const AudioBuffers>);

	/** Called when a processing run has finished */
	void process_end ();

	float current_encoding_rate () const;
	int video_frames_out () const;

private:
	
	void frame_done ();
	
	void encoder_thread (ServerDescription *);
	void terminate_threads ();

	/** Film that we are encoding */
	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Job> _job;

	/** Mutex for _time_history and _last_frame */
	mutable boost::mutex _history_mutex;
	/** List of the times of completion of the last _history_size frames;
	    first is the most recently completed.
	*/
	std::list<struct timeval> _time_history;
	/** Number of frames that we should keep history for */
	static int const _history_size;

	/** Number of video frames received so far */
	ContentVideoFrame _video_frames_in;
	/** Number of video frames written for the DCP so far */
	int _video_frames_out;

#if HAVE_SWRESAMPLE	
	SwrContext* _swr_context;
#endif

	bool _have_a_real_frame;
	bool _terminate;
	std::list<boost::shared_ptr<DCPVideoFrame> > _queue;
	std::list<boost::thread *> _threads;
	mutable boost::mutex _mutex;
	boost::condition _condition;

	boost::shared_ptr<Writer> _writer;
};

#endif

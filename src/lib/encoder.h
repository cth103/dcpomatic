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

#ifndef DVDOMATIC_ENCODER_H
#define DVDOMATIC_ENCODER_H

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
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
#include <sndfile.h>
#include "util.h"
#include "video_sink.h"
#include "audio_sink.h"

class Image;
class Subtitle;
class AudioBuffers;
class Film;
class ServerDescription;
class DCPVideoFrame;

/** @class Encoder
 *  @brief Encoder to J2K and WAV for DCP.
 *
 *  Video is supplied to process_video as YUV frames, and audio
 *  is supplied as uncompressed PCM in blocks of various sizes.
 */

class Encoder : public VideoSink, public AudioSink
{
public:
	Encoder (boost::shared_ptr<const Film> f);
	virtual ~Encoder ();

	/** Called to indicate that a processing run is about to begin */
	virtual void process_begin ();

	/** Call with a frame of video.
	 *  @param i Video frame image.
	 *  @param same true if i is the same as the last time we were called.
	 *  @param s A subtitle that should be on this frame, or 0.
	 */
	void process_video (boost::shared_ptr<Image> i, bool same, boost::shared_ptr<Subtitle> s);

	/** Call with some audio data */
	void process_audio (boost::shared_ptr<AudioBuffers>);

	/** Called when a processing run has finished */
	virtual void process_end ();

	float current_frames_per_second () const;
	bool skipping () const;
	int video_frames_out () const;

private:
	
	void frame_done ();
	void frame_skipped ();
	
	void close_sound_files ();
	void write_audio (boost::shared_ptr<const AudioBuffers> audio);

	void encoder_thread (ServerDescription *);
	void terminate_worker_threads ();
	void link (std::string, std::string) const;

	/** Film that we are encoding */
	boost::shared_ptr<const Film> _film;

	/** Mutex for _time_history, _just_skipped and _last_frame */
	mutable boost::mutex _history_mutex;
	/** List of the times of completion of the last _history_size frames;
	    first is the most recently completed.
	*/
	std::list<struct timeval> _time_history;
	/** Number of frames that we should keep history for */
	static int const _history_size;
	/** true if the last frame we processed was skipped (because it was already done) */
	bool _just_skipped;

	/** Number of video frames received so far */
	SourceFrame _video_frames_in;
	/** Number of audio frames received so far */
	int64_t _audio_frames_in;
	/** Number of video frames written for the DCP so far */
	int _video_frames_out;
	/** Number of audio frames written for the DCP so far */
	int64_t _audio_frames_out;

#if HAVE_SWRESAMPLE	
	SwrContext* _swr_context;
#endif

	/** List of links that we need to create when all frames have been processed;
	 *  such that we need to call link (first, second) for each member of this list.
	 *  In other words, `first' is a `real' frame and `second' should be a link to `first'.
	 *  Frames are DCP frames.
	 */
	std::list<std::pair<int, int> > _links_required;

	std::vector<SNDFILE*> _sound_files;

	boost::optional<int> _last_real_frame;
	bool _process_end;
	std::list<boost::shared_ptr<DCPVideoFrame> > _queue;
	std::list<boost::thread *> _worker_threads;
	mutable boost::mutex _worker_mutex;
	boost::condition _worker_condition;
};

#endif
